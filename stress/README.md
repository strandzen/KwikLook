# Stress tests

Tools to find crashes, races, leaks and hangs that the normal tests don't reach.
Everything runs away from your desktop: the switching test in a nested KWin session
on a private D-Bus with a throwaway home folder, the fuzzers with a throwaway home
folder. Results go to `stress/reports/` (not in git).

## Fast switching (`switching.sh`)

Opens random sets of the test files (pictures, RAW, PDFs, books, archives, office
documents, videos, fonts, cards, text, edge cases), then switches with Next / Previous
at random 0-80 ms, with now and then a pause long enough for a view to finish loading,
and closes and reopens. Every 10 s it samples the viewer: memory, threads, open files,
and its helper processes (the sandbox, LibreOffice). At the end it checks for a crash,
sanitizer reports, processes left running, and how often the closed viewer wakes up.

```sh
testfiles/generate.sh                              # once: the test files
cmake --preset asan && cmake --build --preset asan  # AddressSanitizer + UBSan build
stress/run-nested.sh switching.sh 30               # 30 minutes; default build/asan
stress/run-nested.sh switching.sh 10 build/release # the same with a normal build
STRESS_KINDS="images pdf" stress/run-nested.sh switching.sh 5 build/release  # some kinds only
```

The report folder has `summary.txt`, `samples.csv` (memory and threads over time),
`actions.log` (every call and how long the viewer took to answer) and the viewer's
log; sanitizer findings land there as `asan.*` / `ubsan.*`.

Reading the results: in the nested session (KWin's virtual screen) any Qt window, even a
bare one with no KwikLook code, starts spinning (Wayland and graphics threads, ~70 %
CPU) after its 4th or 5th hide, checked 2026-11-26 with Qt 6.12.1. So the idle numbers
there aren't meaningful after a few previews, and the spinning slows a run down (fewer
actions); judge idle cost on the real desktop (`/proc/<pid>/task/*/status`,
voluntary_ctxt_switches over 10 s). The nested session also renders in software:
memory is higher than on a real desktop.

Memory under AddressSanitizer is several times higher than
normal (it holds freed memory back on purpose); judge memory growth with the release
build. A steady climb over a long run is a leak; going up and down is the working set.

## Fuzzing the parsers (`fuzz/`)

libFuzzer targets for the code that reads file contents:

| Target | What it feeds |
|---|---|
| `fuzz_epub` | `Epub::parseHelperOutput` + `Epub::build` (any book contents) |
| `fuzz_markdown` | `Markdown::forQt` (checks it never changes the number of lines), the Markdown and HTML heading scanners |
| `fuzz_archivelisting` | `ArchiveListing::parse` and `parts` (the archive helper's output) |
| `fuzz_csv` | `parseDelimited` |
| `fuzz_fileinfo` | `FileInfo::read` and a picture decode on a file of any type (the first byte picks the extension): the libraries that run inside the viewer itself (KFileMetaData, Qt's image plugins, fonts, cards) |

```sh
cmake --preset fuzz && cmake --build --preset fuzz   # clang
cmake --build --preset dev                           # kwiklook-archive, for the seeds
stress/fuzz/run.sh 600                               # 10 minutes per target
stress/fuzz/run.sh 3600 fuzz_fileinfo                # one target, an hour
```

Seeds come from the test files. A finding stops that target and leaves its input in
`reports/fuzz-<date>/<target>/findings/` (`crash-*`, `timeout-*`, `oom-*`); replay it
with `build/fuzz/bin/<target> <file>`. Coverage guidance only reaches our own code:
inside Qt's and KDE's libraries the fuzzer mutates blindly, so give `fuzz_fileinfo`
long runs.

## Not here

ThreadSanitizer: it needs Qt itself built with it, otherwise it reports Qt's own
threads as races. Arch's Qt isn't, so a TSan build mostly shows false alarms.
