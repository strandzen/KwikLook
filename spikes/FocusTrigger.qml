// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import kwiklookspike

// Stands in for Dolphin: Space asks the background viewer to show its window.
Kirigami.ApplicationWindow {
    id: root

    property bool automatic: false

    title: "KwikLook focus test (stand-in for Dolphin)"
    width: Kirigami.Units.gridUnit * 30
    height: Kirigami.Units.gridUnit * 22

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    Component.onCompleted: {
        SpikeHelper.registerTrigger()
        if (automatic) {
            SpikeHelper.runAutomatic(root)
        }
    }

    onActiveChanged: if (active && logModel.count > 0) {
        SpikeHelper.log("Focus is back on this window.")
    }

    Connections {
        target: SpikeHelper
        function onLog(message) {
            logModel.append({ line: message })
            logView.positionViewAtEnd()
        }
    }

    Shortcut {
        sequence: "Space"
        enabled: SpikeHelper.viewerReady
        onActivated: SpikeHelper.showViewer(root, tokenBox.checked)
    }

    pageStack.initialPage: Kirigami.Page {
        title: SpikeHelper.viewerReady ? "Press Space to open the preview" : "Starting the viewer…"

        ColumnLayout {
            anchors.fill: parent

            QQC2.CheckBox {
                id: tokenBox
                checked: true
                text: "Pass a focus token (what KwikLook would do)"
            }
            QQC2.Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Kirigami.Theme.disabledTextColor
                text: "Then press Space or Esc in the preview to hide it. Try several times, with and without the token. Expected: with the token the preview gets focus every time; without it, it may not."
            }
            QQC2.ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                ListView {
                    id: logView
                    model: ListModel { id: logModel }
                    delegate: QQC2.Label {
                        required property string line
                        width: ListView.view.width
                        wrapMode: Text.Wrap
                        text: "• " + line
                    }
                }
            }
        }
    }
}
