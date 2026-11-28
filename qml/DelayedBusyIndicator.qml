// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2

// A busy indicator that shows only when the wait is noticeable (300 ms): fast loads
// never flicker.
QQC2.BusyIndicator {
    id: root

    property bool busy: false
    // The wait has lasted long enough to say so (also for text shown with it).
    readonly property bool shown: busy && delay.passed

    running: shown
    visible: running

    Timer {
        id: delay
        property bool passed: false
        interval: 300
        running: root.busy
        onRunningChanged: passed = false
        onTriggered: passed = true
    }
}
