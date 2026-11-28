// SPDX-License-Identifier: GPL-2.0-or-later
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
    minimumWidth: Math.max(Kirigami.Units.gridUnit * 18, Math.ceil(titleBar.minimumWidth)) // the bar's buttons fit
    minimumHeight: Kirigami.Units.gridUnit * 10

    // Videos can go fullscreen: then only the video shows, no frame.
    readonly property bool fullScreen: visibility === Window.FullScreen
    readonly property int radius: fullScreen ? 0 : frame.radius
    // A text field has the focus (font sample, search): keys type, they don't move.
    readonly property bool typing: activeFocusItem !== null && activeFocusItem.echoMode !== undefined

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    WindowFrame {
        id: frame
        window: root.fullScreen ? null : root // no shadow spilling onto another screen
    }

    // Fullscreen belongs to one video: the next file shows in the window again.
    Connections {
        target: Viewer
        function onCurrentChanged() {
            if (root.fullScreen) {
                root.showNormal();
            }
        }
    }

    // The pictures next to the current file, decoded ahead and hidden: ← → then finds
    // them ready in Qt's image cache. Loaded as ImageView's stillImage loads them (the
    // cache key: address, sourceSize, autoTransform, fillMode); keep the two in step.
    // A slot takes its next picture only once the one it has is loaded: cancelling a
    // load can crash Qt 6.12 (CLAUDE.md, Known Pitfalls). Nothing while hidden.
    Item {
        visible: false
        Repeater {
            model: 2 // next, previous
            delegate: Image {
                id: ahead
                required property int index
                readonly property var wanted: root.visible && Viewer.picturesAhead[index] ? Viewer.picturesAhead[index] : ({})
                readonly property real dpr: root.devicePixelRatio !== undefined ? root.devicePixelRatio : Screen.devicePixelRatio
                objectName: "pictureAhead"
                asynchronous: true
                cache: true
                autoTransform: true
                fillMode: Image.PreserveAspectFit
                // What the slot holds: the picture's address and the size it's decoded at.
                property string taken: ""
                function take() {
                    if (status === Image.Loading) {
                        return;
                    }
                    const url = wanted.url ? String(wanted.url) : "";
                    const fitted = wanted.width > Screen.width * dpr || wanted.height > Screen.height * dpr;
                    const size = fitted ? Qt.size(Screen.width * dpr, Screen.height * dpr) : undefined;
                    const key = url === "" ? "" : url + (fitted ? " " + size.width + "x" + size.height : "");
                    if (key === taken) {
                        return;
                    }
                    taken = key;
                    source = ""; // no load at the old address with the new size
                    sourceSize = size;
                    source = url;
                }
                onWantedChanged: take()
                onStatusChanged: take()
            }
        }
    }

    // The view stays this long after the window hides (see viewLoader.active).
    property bool keepView: false
    onVisibleChanged: {
        if (visible) {
            keepView = true;
            dropView.stop();
        } else {
            dropView.restart();
        }
    }
    Timer {
        id: dropView
        interval: 5000
        onTriggered: root.keepView = false
    }

    // For KwikLook's KWin script: the room around a picture shown at real size (the bar,
    // the information panel; pictures go edge to edge), so the window can take its shape.
    Binding {
        target: Viewer
        property: "windowChrome"
        // From the parts themselves, not "window minus view": a hidden window's layouts
        // aren't laid out (the view isn't even loaded then), and the script asks while
        // the window appears.
        value: Qt.size(Viewer.infoOpen ? Kirigami.Units.gridUnit * 18 + infoSeparator.implicitWidth : 0, titleBar.implicitHeight)
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
        sequence: KwikLookSettings.openKey // the key that opened it closes it
        enabled: !root.typing
        onActivated: Viewer.Close()
    }
    // Esc leaves fullscreen or closes the find bar first, then closes.
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (root.fullScreen) {
                root.showNormal();
            } else if (Viewer.indexOpen) {
                Viewer.indexOpen = false;
            } else if (Viewer.searchOpen && viewLoader.searchable) {
                Viewer.searchOpen = false;
            } else {
                Viewer.Close();
            }
        }
    }
    // The index sheet: all files as a grid.
    Shortcut {
        sequences: ["Ctrl+Return", "Ctrl+Enter"]
        enabled: Viewer.count > 1
        onActivated: Viewer.indexOpen = !Viewer.indexOpen
    }
    // Find, for the views that have it (text, PDF).
    Shortcut {
        sequences: [StandardKey.Find]
        enabled: viewLoader.searchable
        onActivated: {
            Viewer.searchOpen = true;
            viewLoader.item.focusSearch();
        }
    }
    Shortcut {
        sequences: [StandardKey.FindNext]
        enabled: viewLoader.searchable && Viewer.searchOpen
        onActivated: viewLoader.item.findNext()
    }
    Shortcut {
        sequences: [StandardKey.FindPrevious]
        enabled: viewLoader.searchable && Viewer.searchOpen
        onActivated: viewLoader.item.findPrevious()
    }
    Shortcut {
        sequences: ["Left", "Up"]
        enabled: !root.typing && !Viewer.indexOpen // the index sheet moves itself
        onActivated: Viewer.Previous()
    }
    Shortcut {
        sequences: ["Right", "Down"]
        enabled: !root.typing && !Viewer.indexOpen
        onActivated: Viewer.Next()
    }
    // Enter: open the file in its usual application (the preview then closes).
    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: !root.typing && !Viewer.indexOpen
        onActivated: fileMenus.openWithDefault()
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

        layer.enabled: root.radius > 0
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
                objectName: "titleBar"
                Layout.fillWidth: true
                visible: !root.fullScreen
                window: root
                menus: fileMenus
                sidebarAvailable: viewLoader.item !== null && viewLoader.item.sidebarAvailable === true
                pageCount: viewLoader.item !== null && viewLoader.item.pageCount !== undefined ? viewLoader.item.pageCount : 0
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    // Zoomed pictures and scrolled pages stay inside, not over the bar.
                    clip: true

                    // The view Viewer picked for the current file's type (a QML file next to this one).
                    Loader {
                        id: viewLoader

                        readonly property bool zoomable: item !== null && typeof item.zoomBy === "function"
                        readonly property bool searchable: item !== null && typeof item.findNext === "function"

                        anchors.fill: parent
                        source: Viewer.view ? Qt.resolvedUrl(Viewer.view + ".qml") : ""
                        // A closed preview frees its view a few seconds later (a decoded picture
                        // held 64 MB on a 4K screen, PDF pages, players). Not at once: pages
                        // still rendering would be cancelled (Qt 6.12's image loader can crash
                        // on that, CLAUDE.md) and a quick reopen keeps it.
                        active: root.visible || root.keepView
                    }

                    // The previous file's view, still, over the next one until that is ready
                    // (at most 300 ms), then faded out: no blank or icon page in between.
                    ShaderEffectSource {
                        id: lastView
                        objectName: "lastView"
                        anchors.fill: parent
                        live: false
                        hideSource: false
                        visible: opacity > 0
                        opacity: 0

                        NumberAnimation {
                            id: lastViewFade
                            target: lastView
                            property: "opacity"
                            to: 0
                            duration: Kirigami.Units.shortDuration
                            onFinished: lastView.sourceItem = null // frees the picture
                        }
                        Timer {
                            id: lastViewWait
                            interval: 16
                            repeat: true
                            property int ticks: 0
                            onTriggered: {
                                const item = viewLoader.item;
                                const ready = viewLoader.status === Loader.Ready && item && item.ready !== false;
                                if (ready || ++ticks >= 300 / interval) {
                                    stop();
                                    lastViewFade.start();
                                }
                            }
                        }
                        Connections {
                            target: Viewer
                            function onAboutToSwitch() {
                                lastViewWait.stop();
                                lastViewFade.stop();
                                lastView.sourceItem = viewLoader;
                                lastView.scheduleUpdate();
                                lastView.opacity = 1;
                            }
                            function onCurrentChanged() {
                                if (lastView.opacity > 0) {
                                    lastViewWait.ticks = 0;
                                    lastViewWait.restart();
                                }
                            }
                        }
                    }

                    // Reading the next file takes a while (a remote folder): say so, after a moment.
                    DelayedBusyIndicator {
                        objectName: "switchingBusy"
                        anchors.centerIn: parent
                        z: 3
                        busy: Viewer.switching
                    }

                    // The shown file went away while previewed (moved, renamed or deleted):
                    // what's on screen stays, with a note over its top.
                    Kirigami.InlineMessage {
                        objectName: "goneMessage"
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: Kirigami.Units.largeSpacing
                        z: 2
                        type: Kirigami.MessageType.Warning
                        text: i18nc("@info", "This file was moved or deleted.")
                        visible: Viewer.currentGone
                    }

                    // The index sheet over the view, while open.
                    Loader {
                        anchors.fill: parent
                        z: 1
                        active: Viewer.indexOpen && Viewer.count > 1
                        sourceComponent: IndexSheet {}
                        onLoaded: item.focusGrid()
                    }

                    // Where you are among the files (on opening and each switch) and the zoom
                    // level (while zooming), briefly, in the corner; above a view's own
                    // bottom bar (find bar, player controls).
                    CornerLabel {
                        id: cornerLabel
                        objectName: "cornerLabel"
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.rightMargin: gap
                        anchors.bottomMargin: gap + (viewLoader.item && viewLoader.item.bottomBarHeight !== undefined ? viewLoader.item.bottomBarHeight : 0)
                        z: 3
                        windowRadius: root.radius
                        positionText: i18nc("@info position in the list of files, e.g. 2 / 5", "%1 / %2", Viewer.currentIndex + 1, Viewer.count)
                        zoomText: viewLoader.zoomable ? i18nc("@info zoom level", "%1%", viewLoader.item.zoomPercent) : ""

                        Connections {
                            target: viewLoader.item
                            ignoreUnknownSignals: true
                            function onZoomed() {
                                cornerLabel.flashZoom();
                            }
                        }
                        Connections {
                            target: Viewer
                            function onCurrentChanged() {
                                if (Viewer.count > 1) {
                                    cornerLabel.flashPosition();
                                }
                            }
                        }
                    }
                }

                // The information panel slides in from the right; the preview shrinks beside it.
                Kirigami.Separator {
                    id: infoSeparator
                    Layout.fillHeight: true
                    visible: infoPanel.Layout.preferredWidth > 0
                }
                Item {
                    id: infoPanel
                    Layout.fillHeight: true
                    Layout.preferredWidth: Viewer.infoOpen && !root.fullScreen ? Kirigami.Units.gridUnit * 18 : 0
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
            radius: root.radius
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
        visible: frame.outline && !root.fullScreen
        color: "transparent"
        radius: root.radius
        antialiasing: true
        border.width: 1
        border.color: Qt.rgba(bg.r + (fg.r - bg.r) * 0.2, bg.g + (fg.g - bg.g) * 0.2, bg.b + (fg.b - bg.b) * 0.2, 1)
    }

    ResizeHandles {
        anchors.fill: parent
        window: root
        visible: !root.fullScreen
    }
}
