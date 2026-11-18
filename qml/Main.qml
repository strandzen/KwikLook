// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import kwiklook

// The preview window, hidden until the viewer is asked to open something.
// Frameless: KwikLook draws corners and outline, WindowFrame adds the shadow,
// all following the user's Breeze settings, so it looks like other windows.
Kirigami.ApplicationWindow {
    id: root

    visible: false
    flags: Qt.Window | Qt.FramelessWindowHint
    color: "transparent"
    title: Viewer.fileName
    width: Kirigami.Units.gridUnit * 36
    height: Kirigami.Units.gridUnit * 24
    minimumWidth: Kirigami.Units.gridUnit * 16
    minimumHeight: Kirigami.Units.gridUnit * 10

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    WindowFrame {
        id: frame
        window: root
    }

    Shortcut {
        sequences: ["Space", "Escape"]
        onActivated: Viewer.Close()
    }
    Shortcut {
        sequence: "Left"
        onActivated: Viewer.Previous()
    }
    Shortcut {
        sequence: "Right"
        onActivated: Viewer.Next()
    }

    // Everything inside the frame, clipped to the rounded corners.
    Rectangle {
        id: content
        anchors.fill: parent
        color: Kirigami.Theme.backgroundColor

        layer.enabled: frame.radius > 0
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: mask
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            TitleBar {
                id: titleBar
                Layout.fillWidth: true
                window: root
            }

            // Placeholder until step 4 (a viewer per file type).
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - Kirigami.Units.gridUnit * 4
                    icon.name: "document-preview"
                    text: Viewer.fileName
                    explanation: Viewer.count > 1 ? i18n("File %1 of %2", Viewer.currentIndex + 1, Viewer.count) : ""
                }
            }
        }
    }

    Item {
        id: mask
        anchors.fill: parent
        visible: false
        layer.enabled: true
        Rectangle {
            anchors.fill: parent
            radius: frame.radius
            antialiasing: true
        }
    }

    // Breeze's outline: the title bar colour mixed 20% towards its text colour, a plain
    // per-channel mix (measured in a dark and a light colour scheme; Kirigami's
    // ColorUtils.linearInterpolation gives a different colour).
    Rectangle {
        readonly property color bg: titleBar.Kirigami.Theme.backgroundColor
        readonly property color fg: titleBar.Kirigami.Theme.textColor

        anchors.fill: parent
        visible: frame.outline
        color: "transparent"
        radius: frame.radius
        antialiasing: true
        border.width: 1
        border.color: Qt.rgba(bg.r + (fg.r - bg.r) * 0.2, bg.g + (fg.g - bg.g) * 0.2, bg.b + (fg.b - bg.b) * 0.2, 1)
    }

    ResizeHandles {
        anchors.fill: parent
        window: root
    }
}
