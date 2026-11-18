#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
# Runs a test on a private session bus with scratch XDG folders, so it never
# touches the user's session or settings.  usage: run-with-bus.sh <test-binary>
T=$(mktemp -d)
mkdir -p "$T/data" "$T/config" "$T/cache" "$T/runtime"
chmod 700 "$T/runtime"
export XDG_DATA_HOME="$T/data" XDG_CONFIG_HOME="$T/config" XDG_CACHE_HOME="$T/cache" XDG_RUNTIME_DIR="$T/runtime"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
dbus-run-session -- "$1"
rc=$?
rm -rf "$T"
exit $rc
