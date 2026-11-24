// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
//
// kwiklook-archive: lists what's inside an archive, for the preview. Run inside
// kwiklook-sandbox (it may read the archive and nothing else); no Qt.
//
//   kwiklook-archive FILE [LIMIT] [SECONDS]
//
// Prints one line per entry, at most LIMIT (default 10000), tab-separated:
//   e <kind d|f|l> <size, -1 unknown> <modified, Unix time, -1 unknown> <encrypted 0|1> <path>
// then counts on to the end (or until SECONDS, default 5, are up) and prints
//   end <entries> <unpacked bytes> <status ok|time|locked|error> <message>
// The path has \ → \\, tab → \t, line break → \n, carriage return → \r.
// A single compressed file (notes.txt.gz) is one entry named like the file without
// its last extension, its size counted by unpacking (within the time).

#include <archive.h>
#include <archive_entry.h>

#include <chrono>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace
{
std::string escaped(const char *text)
{
    std::string out;
    for (const char *c = text; *c; ++c) {
        switch (*c) {
        case '\\': out += "\\\\"; break;
        case '\t': out += "\\t"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        default: out += *c;
        }
    }
    return out;
}

// "notes.txt.gz" → "notes.txt"
std::string withoutLastExtension(const char *path)
{
    std::string name = path;
    if (const auto slash = name.rfind('/'); slash != std::string::npos) {
        name = name.substr(slash + 1);
    }
    if (const auto dot = name.rfind('.'); dot != std::string::npos && dot > 0) {
        name = name.substr(0, dot);
    }
    return name;
}

std::string oneLine(const char *text)
{
    std::string out = text ? text : "";
    for (char &c : out) {
        if (c == '\n' || c == '\t' || c == '\r') {
            c = ' ';
        }
    }
    return out;
}
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: kwiklook-archive FILE [LIMIT] [SECONDS]\n");
        return 2;
    }
    // The sandbox starts it with no environment: names are only converted with a
    // UTF-8 locale (else non-ASCII names come out empty).
    if (!std::setlocale(LC_CTYPE, "C.UTF-8")) {
        std::setlocale(LC_CTYPE, "en_US.UTF-8");
    }
    const char *file = argv[1];
    const long limit = argc > 2 ? std::strtol(argv[2], nullptr, 10) : 10000;
    const double seconds = argc > 3 ? std::strtod(argv[3], nullptr) : 5;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);

    archive *a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    archive_read_support_format_raw(a); // last resort: a single compressed file
    if (archive_read_open_filename(a, file, 64 * 1024) != ARCHIVE_OK) {
        std::printf("end\t0\t0\terror\t%s\n", oneLine(archive_error_string(a)).c_str());
        archive_read_free(a);
        return 0;
    }

    long entries = 0;
    long long unpacked = 0;
    std::string status = "ok";
    std::string message;
    archive_entry *entry = nullptr;
    while (true) {
        if (std::chrono::steady_clock::now() > deadline) {
            status = "time";
            break;
        }
        const int r = archive_read_next_header(a, &entry);
        if (r == ARCHIVE_EOF) {
            break;
        }
        if (r == ARCHIVE_FATAL) {
            const char *error = archive_error_string(a);
            // 7-Zip with encrypted names: nothing can be listed without the password.
            status = error && std::strstr(error, "assphrase") ? "locked" : "error";
            message = oneLine(error);
            break;
        }
        const bool raw = archive_format(a) == ARCHIVE_FORMAT_RAW;
        long long size = archive_entry_size_is_set(entry) ? archive_entry_size(entry) : -1;
        if (raw) {
            // Only unpacking tells the size of a compressed stream.
            long long counted = 0;
            static char buffer[64 * 1024];
            la_ssize_t n;
            while ((n = archive_read_data(a, buffer, sizeof buffer)) > 0) {
                counted += n;
                if (std::chrono::steady_clock::now() > deadline) {
                    status = "time";
                    break;
                }
            }
            size = n < 0 || status == "time" ? -1 : counted;
        }
        const mode_t type = archive_entry_filetype(entry);
        const char kind = type == AE_IFDIR ? 'd' : type == AE_IFLNK ? 'l' : 'f';
        const char *path = archive_entry_pathname_utf8(entry);
        if (!path) {
            path = archive_entry_pathname(entry);
        }
        const std::string name = raw ? withoutLastExtension(file) : escaped(path ? path : "");
        if (kind == 'f' && size > 0) {
            unpacked += size;
        }
        if (entries < limit) {
            std::printf("e\t%c\t%lld\t%lld\t%d\t%s\n", kind, size, archive_entry_mtime_is_set(entry) ? static_cast<long long>(archive_entry_mtime(entry)) : -1LL,
                        archive_entry_is_encrypted(entry) ? 1 : 0, name.c_str());
        }
        ++entries;
        if (r == ARCHIVE_WARN && message.empty()) {
            message = oneLine(archive_error_string(a));
        }
        if (raw || status == "time") {
            break;
        }
    }
    std::printf("end\t%ld\t%lld\t%s\t%s\n", entries, unpacked, status.c_str(), message.c_str());
    archive_read_free(a);
    return 0;
}
