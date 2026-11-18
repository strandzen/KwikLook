// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import kwiklookspike

// The background viewer's window: hidden until the trigger asks over D-Bus.
Kirigami.ApplicationWindow {
    id: root

    title: "Preview (focus test)"
    width: Kirigami.Units.gridUnit * 24
    height: Kirigami.Units.gridUnit * 14
    visible: false

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    Component.onCompleted: SpikeHelper.registerViewer(root)

    Shortcut {
        sequences: ["Space", "Escape"]
        onActivated: root.hide()
    }

    pageStack.initialPage: Kirigami.Page {
        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            icon.name: root.active ? "checkmark" : "dialog-warning"
            text: root.active ? "This window has keyboard focus" : "This window does NOT have keyboard focus"
            explanation: "Space or Esc hides it (only works when it has focus)."
        }
    }
}
