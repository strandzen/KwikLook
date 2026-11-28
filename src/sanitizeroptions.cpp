// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
//
// Sanitizer builds only (CMakePresets.json "asan"; added by CMake to the programs that
// run inside kwiklook-sandbox). The sandbox gives them an empty environment, so
// ASAN_OPTIONS doesn't reach them, and it forbids ptrace, which LeakSanitizer needs:
// it would end every run with "LeakSanitizer has encountered a fatal error" (exit 1).
// Memory errors are still caught; leaks are checked by memory growth (stress/).

extern "C" __attribute__((visibility("default"))) const char *__asan_default_options()
{
    return "detect_leaks=0";
}
