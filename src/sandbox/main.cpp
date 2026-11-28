// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>

// kwiklook-sandbox: runs a program locked down, for converters that parse files
// from anywhere (LibreOffice for office previews). No Qt: small, and nothing runs
// before the lockdown but this file.
//
//   kwiklook-sandbox [--read PATH]... [--write PATH]... [--socket-dir PATH]...
//                    [--env KEY=VALUE]... -- PROGRAM [ARGS...]
//
// The program may read the system folders (programs, libraries, fonts, settings
// under /etc), the --read paths, and read and write the --write paths; nothing else
// (Landlock). In a --socket-dir it may only create and delete entries (LibreOffice
// puts its own pipe in /tmp, hard-coded), not read or write files there.
// No network: only local (AF_UNIX) sockets can be made (seccomp), and Landlock lets
// them reach only sockets made inside the sandbox: not the session bus, not X11.
// It starts with an empty environment (plus PATH and the --env values), without
// inherited file descriptors, in its own process group (the caller stops the whole
// conversion by killing the group).
//
// Exit codes: 77 if the kernel's Landlock can't do all of this (the caller must not
// run the program unsandboxed), 78 for a setup error, otherwise the program's own.

#include <linux/landlock.h>
#include <seccomp.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>

namespace
{
constexpr int ExitNoLandlock = 77;
constexpr int ExitSetup = 78;

// Read-only (and executable): what a program needs to start and find fonts.
const char *const SystemPaths[] = {"/usr", "/etc", "/opt", "/bin", "/sbin", "/lib", "/lib64", "/proc", "/sys/devices/system/cpu", "/var/cache/fontconfig"};
// Devices a program may open for reading and writing.
const char *const DevicePaths[] = {"/dev/null", "/dev/zero", "/dev/full", "/dev/random", "/dev/urandom"};

int fail(const char *what)
{
    std::fprintf(stderr, "kwiklook-sandbox: %s: %s\n", what, std::strerror(errno));
    return ExitSetup;
}

long landlockAbi()
{
    return syscall(SYS_landlock_create_ruleset, nullptr, 0, LANDLOCK_CREATE_RULESET_VERSION);
}

// The file system rights this kernel's Landlock knows (newer ABIs add rights).
__u64 handledFsRights(long abi)
{
    __u64 rights = LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_WRITE_FILE | LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR
        | LANDLOCK_ACCESS_FS_REMOVE_DIR | LANDLOCK_ACCESS_FS_REMOVE_FILE | LANDLOCK_ACCESS_FS_MAKE_CHAR | LANDLOCK_ACCESS_FS_MAKE_DIR
        | LANDLOCK_ACCESS_FS_MAKE_REG | LANDLOCK_ACCESS_FS_MAKE_SOCK | LANDLOCK_ACCESS_FS_MAKE_FIFO | LANDLOCK_ACCESS_FS_MAKE_BLOCK
        | LANDLOCK_ACCESS_FS_MAKE_SYM;
    if (abi >= 2) {
        rights |= LANDLOCK_ACCESS_FS_REFER;
    }
    if (abi >= 3) {
        rights |= LANDLOCK_ACCESS_FS_TRUNCATE;
    }
    if (abi >= 5) {
        rights |= LANDLOCK_ACCESS_FS_IOCTL_DEV;
    }
    return rights;
}

// Whether this kernel restricts connecting to pathname UNIX sockets. Asked directly
// (a ruleset handling the right), not by ABI number.
bool supportsResolveUnix()
{
    struct landlock_ruleset_attr attr = {};
    attr.handled_access_fs = LANDLOCK_ACCESS_FS_RESOLVE_UNIX;
    const int fd = int(syscall(SYS_landlock_create_ruleset, &attr, sizeof(attr), 0));
    if (fd < 0) {
        return false;
    }
    close(fd);
    return true;
}

// Allows `rights` below `path` (or on the file itself). Missing paths are skipped.
bool allow(int ruleset, const char *path, __u64 rights)
{
    const int fd = open(path, O_PATH | O_CLOEXEC);
    if (fd < 0) {
        return errno == ENOENT || errno == ENOTDIR;
    }
    struct stat st;
    if (fstat(fd, &st) == 0 && !S_ISDIR(st.st_mode)) {
        // Rights that only make sense for folders can't be given on a file.
        rights &= LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_WRITE_FILE | LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_TRUNCATE
            | LANDLOCK_ACCESS_FS_IOCTL_DEV;
    }
    struct landlock_path_beneath_attr attr = {};
    attr.allowed_access = rights;
    attr.parent_fd = fd;
    const bool ok = syscall(SYS_landlock_add_rule, ruleset, LANDLOCK_RULE_PATH_BENEATH, &attr, 0) == 0;
    close(fd);
    return ok;
}

int restrictFiles(long abi, const std::vector<std::string> &reads, const std::vector<std::string> &writes, const std::vector<std::string> &socketDirs)
{
    // Connecting to UNIX sockets made outside the sandbox (session bus, X11) is denied.
    const __u64 handled = handledFsRights(abi) | LANDLOCK_ACCESS_FS_RESOLVE_UNIX;
    struct landlock_ruleset_attr rulesetAttr = {};
    rulesetAttr.handled_access_fs = handled;
    if (abi >= 4) {
        // No TCP at all (seccomp already refuses sockets; this holds on its own too).
        rulesetAttr.handled_access_net = LANDLOCK_ACCESS_NET_BIND_TCP | LANDLOCK_ACCESS_NET_CONNECT_TCP;
    }
    // No signals to, and no abstract sockets of, processes outside the sandbox.
    rulesetAttr.scoped = LANDLOCK_SCOPE_ABSTRACT_UNIX_SOCKET | LANDLOCK_SCOPE_SIGNAL;
    const int ruleset = int(syscall(SYS_landlock_create_ruleset, &rulesetAttr, sizeof(rulesetAttr), 0));
    if (ruleset < 0) {
        return fail("landlock_create_ruleset");
    }
    const __u64 readOnly = handled & (LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR);
    const __u64 device = handled & (LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_WRITE_FILE | LANDLOCK_ACCESS_FS_TRUNCATE);
    bool ok = true;
    for (const char *path : SystemPaths) {
        ok = ok && allow(ruleset, path, readOnly);
    }
    for (const char *path : DevicePaths) {
        ok = ok && allow(ruleset, path, device);
    }
    for (const std::string &path : reads) {
        ok = ok && allow(ruleset, path.c_str(), readOnly);
    }
    for (const std::string &path : writes) {
        ok = ok && allow(ruleset, path.c_str(), handled);
    }
    for (const std::string &path : socketDirs) {
        ok = ok && allow(ruleset, path.c_str(), LANDLOCK_ACCESS_FS_MAKE_SOCK | LANDLOCK_ACCESS_FS_REMOVE_FILE);
    }
    if (!ok) {
        close(ruleset);
        return fail("landlock_add_rule");
    }
    if (syscall(SYS_landlock_restrict_self, ruleset, 0) != 0) {
        close(ruleset);
        return fail("landlock_restrict_self");
    }
    close(ruleset);
    return 0;
}

int restrictSyscalls()
{
    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ALLOW);
    if (!ctx) {
        return fail("seccomp_init");
    }
    // Only local sockets (AF_UNIX): no network, no netlink. Which local sockets they
    // may reach is up to Landlock (see restrictFiles()).
    if (seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EACCES), SCMP_SYS(socket), 1, SCMP_A0(SCMP_CMP_NE, AF_UNIX)) != 0) {
        seccomp_release(ctx);
        return fail("seccomp_rule_add");
    }
    const int denied[] = {SCMP_SYS(io_uring_setup), SCMP_SYS(io_uring_enter), SCMP_SYS(io_uring_register), SCMP_SYS(ptrace),
                          SCMP_SYS(bpf), SCMP_SYS(perf_event_open), SCMP_SYS(add_key), SCMP_SYS(request_key), SCMP_SYS(keyctl)};
    for (int call : denied) {
        if (seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EACCES), call, 0) != 0) {
            seccomp_release(ctx);
            return fail("seccomp_rule_add");
        }
    }
    const int result = seccomp_load(ctx);
    seccomp_release(ctx);
    if (result != 0) {
        errno = -result;
        return fail("seccomp_load");
    }
    return 0;
}

void usage()
{
    std::fprintf(stderr, "usage: kwiklook-sandbox [--read PATH]... [--write PATH]... [--socket-dir PATH]... [--env KEY=VALUE]... -- PROGRAM [ARGS...]\n");
}
} // namespace

int main(int argc, char **argv)
{
    std::vector<std::string> reads;
    std::vector<std::string> writes;
    std::vector<std::string> socketDirs;
    std::vector<std::string> env{"PATH=/usr/bin:/bin"};
    int i = 1;
    for (; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--") {
            ++i;
            break;
        }
        if (i + 1 >= argc) {
            usage();
            return ExitSetup;
        }
        if (arg == "--read") {
            reads.emplace_back(argv[++i]);
        } else if (arg == "--write") {
            writes.emplace_back(argv[++i]);
        } else if (arg == "--socket-dir") {
            socketDirs.emplace_back(argv[++i]);
        } else if (arg == "--env") {
            env.emplace_back(argv[++i]);
        } else {
            usage();
            return ExitSetup;
        }
    }
    if (i >= argc) {
        usage();
        return ExitSetup;
    }

    // Everything or nothing: without scopes (ABI 6) and the UNIX socket right, the
    // program could reach the session bus or X11, so it isn't run at all.
    const long abi = landlockAbi();
    if (abi < 6 || !supportsResolveUnix()) {
        std::fprintf(stderr, "kwiklook-sandbox: this kernel's Landlock (ABI %ld) can't lock the program in; not running %s\n", abi, argv[i]);
        return ExitNoLandlock;
    }

    setsid(); // own process group: the caller can stop everything the program starts
    const struct rlimit noCore = {0, 0};
    setrlimit(RLIMIT_CORE, &noCore);
    // Close what was inherited (only stdin, stdout and stderr stay).
    if (syscall(SYS_close_range, 3U, ~0U, 0U) != 0) {
        for (int fd = 3; fd < 1024; ++fd) {
            close(fd);
        }
    }
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        return fail("no_new_privs");
    }
    if (const int r = restrictFiles(abi, reads, writes, socketDirs)) {
        return r;
    }
    if (const int r = restrictSyscalls()) {
        return r;
    }

    std::vector<char *> envp;
    for (std::string &e : env) {
        envp.push_back(e.data());
    }
    envp.push_back(nullptr);
    execvpe(argv[i], argv + i, envp.data());
    return fail("exec");
}
