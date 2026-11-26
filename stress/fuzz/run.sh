#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#
# Fuzzes KwikLook's parsers with libFuzzer, starting from the test files.
#
#   stress/fuzz/run.sh [seconds per target] [targets...]
#
# Default: 300 s each, all targets. Build first: cmake --preset fuzz && cmake --build --preset fuzz
# (and kwiklook-archive in build/dev for the seeds). Findings: stress/reports/fuzz-<date>/
# (crash-*, oom-*, timeout-* inputs per target, log.txt). Runs with a throwaway HOME.
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
seconds=${1:-300}
shift || true
targets=("${@:-fuzz_epub fuzz_markdown fuzz_archivelisting fuzz_csv fuzz_fileinfo}")
targets=(${targets[@]})
bin="$root/build/fuzz/bin"
helper="$root/build/dev/bin/kwiklook-archive"
tf="$root/testfiles"
out="$root/stress/reports/fuzz-$(date +%Y-%m-%d-%H%M%S)"
mkdir -p "$out"
scratch=$(mktemp -d "${TMPDIR:-/tmp}/kwiklook-fuzz.XXXXXX")
trap 'rm -rf "$scratch"' EXIT
mkdir -p "$scratch/home"

seed() { # seed <target>: the starting inputs, in $out/<target>/corpus
    local dir="$out/$1/corpus" f i=0
    mkdir -p "$dir"
    case $1 in
    fuzz_epub)
        for f in "$tf"/books/*.epub; do
            "$helper" --files "$f" 2000000 8000000 .xml .opf .ncx .xhtml .html .htm .png .jpg .svg > "$dir/$(basename "$f").out" 2>/dev/null
        done ;;
    fuzz_archivelisting)
        for f in "$tf"/archives/*; do "$helper" "$f" 200 2 > "$dir/$(basename "$f").out" 2>/dev/null; done ;;
    fuzz_markdown)
        cp "$tf"/text/*.md "$tf"/text/*.html "$root/README.md" "$dir/" 2>/dev/null ;;
    fuzz_csv)
        find "$tf/text" -name '*.csv' -size -300k -exec cp {} "$dir/" \; ;;
    fuzz_fileinfo)
        # Keep in step with the extensions in fuzz_fileinfo.cpp: the first byte picks one.
        local exts=(jpg png gif webp svg tiff bmp ico avif jxl psd tga pdf mp3 flac ogg mp4 mkv ttf otf md html json csv desktop vcf ics txt docx odt xlsx zip dng heic exr qoi)
        for f in $(find "$tf/images" "$tf/pdf" "$tf/media" "$tf/fonts" "$tf/cards" "$tf/text" "$tf/office" -type f -size -600k 2>/dev/null); do
            local ext="${f##*.}" n
            for n in "${!exts[@]}"; do
                if [ "${exts[$n]}" = "${ext,,}" ]; then
                    { printf "\\x$(printf %02x "$n")"; cat "$f"; } > "$dir/$((i++)).$ext"
                fi
            done
        done ;;
    esac
}

for t in "${targets[@]}"; do
    [ -x "$bin/$t" ] || { echo "$t: not built" | tee -a "$out/log.txt"; continue; }
    seed "$t"
    mkdir -p "$out/$t/findings"
    echo "== $t: $(ls "$out/$t/corpus" | wc -l) seeds, $seconds s" | tee -a "$out/log.txt"
    (cd "$out/$t" && env -i PATH="$PATH" HOME="$scratch/home" XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp}" \
        ASAN_OPTIONS="detect_leaks=0:allocator_may_return_null=1" UBSAN_OPTIONS="print_stacktrace=1" \
        "$bin/$t" -max_total_time="$seconds" -timeout=10 -rss_limit_mb=4096 -max_len=2000000 \
        -artifact_prefix="$out/$t/findings/" "$out/$t/corpus" > "$out/$t/fuzz.log" 2>&1)
    grep -E "^#[0-9]+ +DONE|cov:|ERROR|SUMMARY|Test unit written" "$out/$t/fuzz.log" | tail -3 | tee -a "$out/log.txt"
    echo "findings: $(ls "$out/$t/findings" | wc -l)" | tee -a "$out/log.txt"
done
echo "Report: $out"
