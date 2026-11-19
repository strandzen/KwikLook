// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
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
    minimumWidth: Kirigami.Units.gridUnit * 18
    minimumHeight: Kirigami.Units.gridUnit * 10

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    WindowFrame {
        id: frame
        window: root
    }

    // “Open with…”, Share and the panel's actions, for the bar and the panel.
    FileMenus {
        id: fileMenus
        url: Viewer.currentUrl
        mimeType: Viewer.mimeType
        onLaunched: Viewer.Close()
        onMessage: text => root.showPassiveNotification(text)
    }

    Shortcut {
        sequences: [KwikLookSettings.openKey, "Escape"] // the key that opened it closes it
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
    // Zoom, for the views that have it (pictures, text, PDF).
    Shortcut {
        sequences: [StandardKey.ZoomIn, "Ctrl+="]
        enabled: viewLoader.zoomable
        onActivated: viewLoader.item.zoomBy(1.25)
    }
    Shortcut {
        sequences: [StandardKey.ZoomOut]
        enabled: viewLoader.zoomable
        onActivated: viewLoader.item.zoomBy(1 / 1.25)
    }
    Shortcut {
        sequence: "Ctrl+0"
        enabled: viewLoader.zoomable
        onActivated: viewLoader.item.resetZoom()
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
                menus: fileMenus
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    // The view Viewer picked for the current file's type (a QML file next to this one).
                    Loader {
                        id: viewLoader

                        readonly property bool zoomable: item !== null && typeof item.zoomBy === "function"

                        anchors.fill: parent
                        source: Viewer.view ? Qt.resolvedUrl(Viewer.view + ".qml") : ""
                    }

                    // The zoom level, shown briefly while zooming.
                    Rectangle {
                        id: zoomIndicator
                        objectName: "zoomIndicator"
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: Kirigami.Units.largeSpacing * 2
                        width: zoomLabel.implicitWidth + Kirigami.Units.largeSpacing * 2
                        height: zoomLabel.implicitHeight + Kirigami.Units.smallSpacing * 2
                        radius: Kirigami.Units.cornerRadius
                        color: Kirigami.Theme.backgroundColor
                        opacity: 0
                        visible: opacity > 0
                        Behavior on opacity {
                            NumberAnimation {
                                duration: Kirigami.Units.longDuration
                            }
                        }

                        QQC2.Label {
                            id: zoomLabel
                            anchors.centerIn: parent
                            text: viewLoader.zoomable ? i18nc("@info zoom level", "%1%", viewLoader.item.zoomPercent) : ""
                        }
                        Timer {
                            id: zoomIndicatorTimer
                            interval: 1000
                            onTriggered: zoomIndicator.opacity = 0
                        }
                        Connections {
                            target: viewLoader.item
                            ignoreUnknownSignals: true
                            function onZoomed() {
                                zoomIndicator.opacity = 1;
                                zoomIndicatorTimer.restart();
                            }
                        }
                    }
                }

                // The information panel slides in from the right; the preview shrinks beside it.
                Kirigami.Separator {
                    Layout.fillHeight: true
                    visible: infoPanel.Layout.preferredWidth > 0
                }
                Item {
                    id: infoPanel
                    Layout.fillHeight: true
                    Layout.preferredWidth: Viewer.infoOpen ? Kirigami.Units.gridUnit * 18 : 0
                    Behavior on Layout.preferredWidth {
                        NumberAnimation {
                            duration: Kirigami.Units.longDuration
                            easing.type: Easing.InOutQuad
                        }
                    }
                    visible: Layout.preferredWidth > 0
                    clip: true

                    InfoPanel {
                        // Full width while sliding, so the text doesn't reflow.
                        width: Kirigami.Units.gridUnit * 18
                        height: parent.height
                        window: root
                        menus: fileMenus
                    }
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
