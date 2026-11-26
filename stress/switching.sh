#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#
# Fast switching through every kind of test file, run by run-nested.sh inside a nested
# session. Opens random sets of files, then Next / Previous at random 0-80 ms with now
# and then a pause long enough for a view to finish loading, and closes and reopens.
# Every 10 s it samples the viewer (memory, threads, open files, helper processes); at
# the end it checks for crashes, sanitizer reports, left-over helpers and idle CPU.
set -uo pipefail
busiest=""
cpuIdle=""
root=$STRESS_ROOT build=$STRESS_BUILD report=$STRESS_REPORT
end=$(( $(date +%s) + STRESS_MINUTES * 60 ))
export KWIKLOOK_DISABLE_DOLPHIN_HOOK=1 QT_FORCE_STDERR_LOGGING=1
export ASAN_OPTIONS="detect_leaks=0:log_path=$report/asan:print_suppressions=0"
export UBSAN_OPTIONS="print_stacktrace=1:log_path=$report/ubsan"
dbus-update-activation-environment WAYLAND_DISPLAY KWIKLOOK_DISABLE_DOLPHIN_HOOK QT_FORCE_STDERR_LOGGING ASAN_OPTIONS UBSAN_OPTIONS

# STRESS_KINDS: only these test file folders, e.g. "images pdf" (default: all of them).
kinds=${STRESS_KINDS:-images pdf books archives office media fonts text cards many edge}
paths=()
for k in $kinds; do paths+=(-o -path "*/$k/*"); done
mapfile -t files < <(find "$root/testfiles" -type f -not -path '*/demo/*' -not -name '*.sh' -not -name '*.md' -size -200M -not -name "*'*" -not -name '*"*' \
    \( -false "${paths[@]}" \) | shuf)
# The viewer's own helpers (the sandbox, the archive lister, LibreOffice), not your
# session's: everything below the viewer's process.
descendants() {
    local p
    for p in $(pgrep -P "$1" 2>/dev/null); do echo "$p"; descendants "$p"; done
}
helperCount() { # helperCount <pid> <pattern>
    local n=0 p
    for p in $(descendants "$1"); do grep -qa -- "$2" /proc/$p/cmdline 2>/dev/null && n=$((n + 1)); done
    echo $n
}
# Each call is logged with how long the viewer took to answer (actions.log), so a run
# can be followed up to a crash or a hang.
viewer() {
    local t0=$(date +%s%3N) result
    timeout 10 gdbus call --session -d io.github.strandzen.KwikLook -o /Viewer -m io.github.strandzen.KwikLook.Viewer."$@" >/dev/null 2>&1
    result=$?
    echo "$(date +%T) $(( $(date +%s%3N) - t0 )) ms exit $result: ${*:0:3}" | cut -c1-300 >> "$report/actions.log"
}
pick() { # pick <n>: a D-Bus string list of n random test files
    local list="" f
    for f in $(printf '%s\n' "${files[@]}" | shuf -n "$1" | sed 's/ /%20/g'); do list+="'file://$f', "; done
    echo "[${list%, }]"
}
sample() {
    local pid=$1 rss threads fds helpers office
    rss=$(awk '/^VmRSS/{print $2}' /proc/$pid/status 2>/dev/null)
    threads=$(awk '/^Threads/{print $2}' /proc/$pid/status 2>/dev/null)
    fds=$(ls /proc/$pid/fd 2>/dev/null | wc -l)
    helpers=$(helperCount "$pid" kwiklook-sandbox)
    office=$(helperCount "$pid" soffice)
    echo "$(date +%s),$rss,$threads,$fds,${helpers:-0},${office:-0},$actions" >> "$report/samples.csv"
}

echo "time,rss_kb,threads,fds,sandbox_helpers,libreoffice,actions" > "$report/samples.csv"
# STRESS_WRAPPER: a command to run the viewer under, e.g. "heaptrack -o $report/heap".
$STRESS_WRAPPER "$build/bin/kwiklook" --service > "$report/viewer.log" 2>&1 &
pid=$!
if [ -n "${STRESS_WRAPPER:-}" ]; then # the viewer is a child of the wrapper: find it
    for _ in $(seq 1 60); do
        for c in $(descendants "$pid"); do [ "$(cat /proc/$c/comm 2>/dev/null)" = kwiklook ] && viewerPid=$c; done
        [ -n "${viewerPid:-}" ] && break; sleep 0.5
    done
    pid=${viewerPid:-$pid}
fi
# Wait until this viewer owns the name: a call before that would make D-Bus start the
# installed one (its service file) instead.
owner() {
    gdbus call --session -d org.freedesktop.DBus -o /org/freedesktop/DBus -m org.freedesktop.DBus.GetConnectionUnixProcessID \
        io.github.strandzen.KwikLook 2>/dev/null | grep -o '[0-9]\+' | tail -1
}
for _ in $(seq 1 120); do [ "$(owner)" = "$pid" ] && break; kill -0 "$pid" 2>/dev/null || break; sleep 0.5; done
if [ "$(owner)" != "$pid" ]; then
    echo "The viewer under test didn't get its D-Bus name (owner: $(owner))." > "$report/summary.txt"
    kill "$pid" 2>/dev/null; exit 1
fi
actions=0 crashed=0 lastSample=0
viewer Open "$(pick 30)" 0 "" ""
while [ "$(date +%s)" -lt "$end" ]; do
    if ! kill -0 "$pid" 2>/dev/null; then
        wait "$pid"; echo "The viewer ended with status $?." >> "$report/actions.log"
        crashed=1; break
    fi
    r=$((RANDOM % 100))
    if   [ $r -lt 45 ]; then viewer Next
    elif [ $r -lt 75 ]; then viewer Previous
    elif [ $r -lt 85 ]; then viewer Open "$(pick $((RANDOM % 40 + 2)))" $((RANDOM % 2)) "" ""
    elif [ $r -lt 92 ]; then viewer Close; sleep 0.$((RANDOM % 5)); viewer Open "$(pick 20)" 0 "" ""
    else sleep 1.$((RANDOM % 5)); fi # let a view finish loading (PDF pages, office, video)
    actions=$((actions + 1))
    sleep "0.0$((RANDOM % 8))"
    now=$(date +%s)
    if [ $((now - lastSample)) -ge 10 ]; then sample "$pid"; lastSample=$now; fi
done

# The end: closed and idle.
viewer Close; sleep 5
leftover=""
for p in $(descendants "$pid"); do leftover+="$(tr '\0' ' ' < /proc/$p/cmdline 2>/dev/null | cut -c1-120); "; done
idle="-"
if kill -0 "$pid" 2>/dev/null; then
    sample "$pid"
    # Wake-ups per thread over 10 s, by thread name: the busiest ones go in the summary.
    wakeups() { for t in /proc/$pid/task/*; do printf '%s %s %s\n' "${t##*/}" "$(cat $t/comm 2>/dev/null | tr ' ' _)" "$(awk '/^voluntary_ctxt_switches/{print $2}' $t/status 2>/dev/null)"; done; }
    cpu() { awk '{print $14 + $15}' /proc/$pid/stat 2>/dev/null; }
    c0=$(cpu); wakeups > "$report/.w0"; sleep 10; wakeups > "$report/.w1"; c1=$(cpu)
    cpuIdle=$(( (c1 - c0) * 10 )) # CPU time in ms (clock ticks of 10 ms)
    busiest=$(join <(sort "$report/.w0") <(sort "$report/.w1") | awk '{d=$5-$3; if (d > 0) print d, $2}' | sort -rn | head -5 | tr '\n' ';')
    idle=$(join <(sort "$report/.w0") <(sort "$report/.w1") | awk '{s+=$5-$3} END{print s+0}')
    rm -f "$report/.w0" "$report/.w1"
    if [ -n "${STRESS_KILL:-}" ]; then
        # Ended as it is: a heap profiler then sees what the closed viewer holds (a
        # normal quit would free Qt's caches first).
        kill -9 "$pid"; sleep 5
    else
        "$build/bin/kwiklook" --quit; sleep 2; kill "$pid" 2>/dev/null
    fi
else
    crashed=1
fi
first=$(sed -n 2p "$report/samples.csv" | cut -d, -f2) last=$(tail -1 "$report/samples.csv" | cut -d, -f2)
maxrss=$(tail -n +2 "$report/samples.csv" | cut -d, -f2 | sort -n | tail -1)
reports=$(ls "$report"/asan* "$report"/ubsan* 2>/dev/null | wc -l)
{
    echo "Ran $STRESS_MINUTES min, $actions actions over ${#files[@]} test files ($kinds)."
    [ "$crashed" = 1 ] && echo "CRASHED: the viewer died (see viewer.log, and asan.* / ubsan.* here)." || echo "No crash."
    echo "Sanitizer reports: $reports file(s)."
    echo "Memory (RSS): first sample ${first:-?} KB, last ${last:-?} KB, highest ${maxrss:-?} KB."
    echo "Idle after closing: $idle wake-ups in 10 s, CPU ${cpuIdle:-?} ms${busiest:+ (busiest threads: $busiest)}."
    echo "  (In the nested session any Qt window spins after a few show/hide cycles, KwikLook or not:"
    echo "  judge idle cost on the real desktop. See stress/README.md.)"
    [ -n "$leftover" ] && echo "LEFT RUNNING after closing: $leftover" || echo "No helper processes left running."
} > "$report/summary.txt"
