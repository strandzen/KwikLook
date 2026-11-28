#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#
# Runs one of the stress scripts in a nested KWin session on a private D-Bus, with a
# throwaway home and config folder: nothing reaches your desktop session, your
# settings or your files.
#
#   stress/run-nested.sh switching.sh [minutes] [build folder]
#
# Defaults: 10 minutes, build/asan (cmake --preset asan && cmake --build --preset asan).
# The report goes to stress/reports/<date>/ (summary.txt, samples.csv, sanitizer logs).
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
script=${1:?which script, e.g. switching.sh}
minutes=${2:-10}
build=$(cd "$root/${3:-build/asan}" && pwd)
[ -x "$build/bin/kwiklook" ] || { echo "No viewer in $build: build it first" >&2; exit 1; }
[ -d "$root/testfiles/images" ] || { echo "No test files: run testfiles/generate.sh first" >&2; exit 1; }
report="$here/reports/$(date +%Y-%m-%d-%H%M%S)"
scratch=$(mktemp -d "${TMPDIR:-/tmp}/kwiklook-stress.XXXXXX")
mkdir -p "$report" "$scratch"/{home,config,cache,state,data}
trap 'rm -rf "$scratch"' EXIT
# KWin's scripts stay off (they move windows); nothing is copied from your config.
printf '[Plugins]\n' > "$scratch/config/kwinrc"
printf '[General]\nWindowPosition=Screen\nCloseOnFocusLoss=false\n' > "$scratch/config/kwiklookrc"
env -i PATH="$PATH" HOME="$scratch/home" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" \
    XDG_CONFIG_HOME="$scratch/config" XDG_CACHE_HOME="$scratch/cache" XDG_STATE_HOME="$scratch/state" XDG_DATA_HOME="$scratch/data" \
    STRESS_ROOT="$root" STRESS_BUILD="$build" STRESS_REPORT="$report" STRESS_MINUTES="$minutes" STRESS_KINDS="${STRESS_KINDS:-}" STRESS_WRAPPER="${STRESS_WRAPPER:-}" STRESS_KILL="${STRESS_KILL:-}" \
    dbus-run-session -- kwin_wayland --virtual --width 1600 --height 1000 --exit-with-session "$here/$script" > "$report/kwin.log" 2>&1 || true
echo "Report: $report"
cat "$report/summary.txt" 2>/dev/null || echo "(no summary: see $report/kwin.log)"
