// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>

// Runs inside kwiklook-sandbox for tst_sandbox: each command tries one thing and
// prints "ok" or "denied <errno>".
//   sandboxprobe read FILE | write FILE | list DIR | tcp | udp | unix PATH | abstract
//                | selfsocket PATH | env NAME | fds
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static int report(bool ok)
{
    if (ok) {
        std::puts("ok");
    } else {
        std::printf("denied %d\n", errno);
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        return 2;
    }
    const std::string cmd = argv[1];
    const char *arg = argc > 2 ? argv[2] : "";
    if (cmd == "read") {
        const int fd = open(arg, O_RDONLY);
        char c;
        return report(fd >= 0 && read(fd, &c, 1) >= 0);
    }
    if (cmd == "write") {
        const int fd = open(arg, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        return report(fd >= 0 && write(fd, "x", 1) == 1);
    }
    if (cmd == "list") {
        DIR *dir = opendir(arg);
        return report(dir != nullptr);
    }
    if (cmd == "tcp" || cmd == "udp") {
        const int fd = socket(AF_INET, cmd == "tcp" ? SOCK_STREAM : SOCK_DGRAM, 0);
        if (fd < 0) {
            return report(false);
        }
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(9);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        return report(connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0 || errno == ECONNREFUSED);
    }
    if (cmd == "unix" || cmd == "abstract") {
        const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
            return report(false);
        }
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        if (cmd == "unix") {
            std::strncpy(addr.sun_path, arg, sizeof(addr.sun_path) - 1);
        } else {
            std::strcpy(addr.sun_path + 1, "kwiklook-probe");
        }
        return report(connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0);
    }
    if (cmd == "selfsocket") {
        // Listen on PATH and connect to it, then remove it (like LibreOffice's pipe).
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, arg, sizeof(addr.sun_path) - 1);
        const int server = socket(AF_UNIX, SOCK_STREAM, 0);
        const int client = socket(AF_UNIX, SOCK_STREAM, 0);
        const bool ok = server >= 0 && client >= 0 && bind(server, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0
            && listen(server, 1) == 0 && connect(client, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0 && unlink(arg) == 0;
        return report(ok);
    }
    if (cmd == "env") {
        const char *value = std::getenv(arg);
        std::printf("%s\n", value ? value : "(unset)");
        return 0;
    }
    if (cmd == "fds") {
        int open = 0;
        for (int fd = 3; fd < 256; ++fd) {
            if (fcntl(fd, F_GETFD) != -1) {
                ++open;
            }
        }
        std::printf("%d\n", open);
        return 0;
    }
    return 2;
}
