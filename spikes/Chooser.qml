// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import kwiklookspike

Kirigami.ApplicationWindow {
    id: root

    property bool openAll: false

    title: "KwikLook frame test"
    width: Kirigami.Units.gridUnit * 26
    height: Kirigami.Units.gridUnit * 20

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    Component.onCompleted: if (openAll) {
        SpikeHelper.openVariant("native")
        SpikeHelper.openVariant("notitle")
        SpikeHelper.openVariant("frameless")
    }

    pageStack.initialPage: Kirigami.ScrollablePage {
        title: "Which window frame looks right?"

        ColumnLayout {
            spacing: Kirigami.Units.largeSpacing

            QQC2.Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: "Open the variants and compare corners, shadow and outline with your other windows. Space or Esc closes a preview; drag the toolbar to move one."
            }

            Kirigami.FormLayout {
                Layout.fillWidth: true

                QQC2.Button {
                    Kirigami.FormData.label: "A:"
                    text: "Native frame + toolbar"
                    onClicked: SpikeHelper.openVariant("native")
                }
                QQC2.Button {
                    Kirigami.FormData.label: "B:"
                    text: "Breeze, title bar hidden"
                    onClicked: SpikeHelper.openVariant("notitle")
                }
                QQC2.Button {
                    Kirigami.FormData.label: "C:"
                    text: "Frameless, our own shadow"
                    onClicked: SpikeHelper.openVariant("frameless")
                }
            }

            QQC2.Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Kirigami.Theme.disabledTextColor
                text: "B needs a Breeze override first: System Settings → Colours & Themes → Window Decorations → Breeze (edit) → Window-Specific Overrides → Add. Regular expression: “KwikLook\\.Spike\\.NoTitle”, tick “Hide window title bar”. Without it, B looks like A."
            }
        }
    }
}
