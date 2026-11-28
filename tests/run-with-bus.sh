#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
# Runs a test on a private session bus with scratch XDG folders, so it never
# touches the user's session or settings.  usage: run-with-bus.sh <test-binary> [test functions]
T=$(mktemp -d)
mkdir -p "$T/data" "$T/config" "$T/cache" "$T/state" "$T/runtime"
chmod 700 "$T/runtime"
export XDG_DATA_HOME="$T/data" XDG_CONFIG_HOME="$T/config" XDG_CACHE_HOME="$T/cache" XDG_STATE_HOME="$T/state" XDG_RUNTIME_DIR="$T/runtime"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
# A bus without service folders: nothing installed can be started on it (KIO's
# plugins woke kdeconnectd and kactivitymanagerd, which outlived the test).
cat > "$T/bus.conf" <<EOF
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN" "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:dir=$T/runtime</listen>
  <policy context="default"><allow send_destination="*" eavesdrop="true"/><allow eavesdrop="true"/><allow own="*"/></policy>
</busconfig>
EOF
dbus-run-session --config-file="$T/bus.conf" -- "$@"
rc=$?
rm -rf "$T"
exit $rc
