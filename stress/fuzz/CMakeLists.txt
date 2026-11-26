# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#
# Fuzz targets (libFuzzer, clang only): preset "fuzz" turns this on. See stress/README.md.
foreach(target fuzz_epub fuzz_markdown fuzz_archivelisting fuzz_csv fuzz_fileinfo)
    add_executable(${target} ${target}.cpp)
    target_link_libraries(${target} PRIVATE kwiklook_core Qt6::Gui)
    target_link_options(${target} PRIVATE -fsanitize=fuzzer)
endforeach()
