// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import kwiklookspike

// One preview window in one of three frame variants:
//  native    - KWin/Breeze draws title bar, corners and shadow; our toolbar sits below
//  notitle   - same, but a Breeze override hides the title bar for this app
//  frameless - no decoration at all; we draw corners, outline and shadow ourselves
Kirigami.ApplicationWindow {
    id: root

    property string variant: SpikeHelper.mode
    readonly property bool frameless: variant === "frameless"
    // Measured on a Breeze window in the nested session (step 1).
    readonly property int cornerRadius: 4

    title: "Horos.png"
    width: Kirigami.Units.gridUnit * 30
    height: Kirigami.Units.gridUnit * 20
    flags: frameless ? (Qt.Window | Qt.FramelessWindowHint) : Qt.Window
    color: frameless ? "transparent" : Kirigami.Theme.backgroundColor

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    Component.onCompleted: if (frameless) SpikeHelper.applyShadow(root, cornerRadius)

    Shortcut {
        sequences: ["Space", "Escape"]
        onActivated: root.close()
    }

    Rectangle {
        id: frame
        anchors.fill: parent
        radius: root.frameless ? root.cornerRadius : 0
        color: Kirigami.Theme.backgroundColor
        // Breeze's faint outline around windows (visible in dark themes).
        border.width: root.frameless ? 1 : 0
        border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.25) // matched to Breeze in the nested session

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: frame.border.width
            spacing: 0

            QQC2.ToolBar {
                Layout.fillWidth: true
                // Frameless and notitle windows have no title bar to drag.
                DragHandler {
                    target: null
                    onActiveChanged: if (active) root.startSystemMove()
                }
                RowLayout {
                    anchors.fill: parent
                    QQC2.ToolButton {
                        icon.name: "go-previous"
                        QQC2.ToolTip.text: "Previous file"
                        QQC2.ToolTip.visible: hovered
                    }
                    QQC2.ToolButton {
                        icon.name: "go-next"
                        QQC2.ToolTip.text: "Next file"
                        QQC2.ToolTip.visible: hovered
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideMiddle
                        // The native variant already shows the name in KWin's title bar.
                        text: root.variant === "native" ? "" : root.title
                    }
                    QQC2.ToolButton {
                        icon.name: "document-open"
                        text: "Open with…"
                    }
                    QQC2.ToolButton {
                        icon.name: "document-share"
                        QQC2.ToolTip.text: "Share"
                        QQC2.ToolTip.visible: hovered
                    }
                    QQC2.ToolButton {
                        visible: root.variant !== "native"
                        icon.name: "window-close"
                        onClicked: root.close()
                    }
                }
            }

            // Image clipped to the window's rounded bottom corners (frameless only).
            Kirigami.ShadowedTexture {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "transparent"
                corners.bottomLeftRadius: root.frameless ? root.cornerRadius - frame.border.width : 0
                corners.bottomRightRadius: root.frameless ? root.cornerRadius - frame.border.width : 0
                source: Image {
                    // Only a texture for ShadowedTexture; not drawn by itself.
                    visible: false
                    width: parent ? parent.width : 0
                    height: parent ? parent.height : 0
                    source: SpikeHelper.sampleImage
                    sourceSize.width: 1280
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                }
            }
        }
    }
}
