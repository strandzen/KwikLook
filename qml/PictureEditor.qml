// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami
import kwiklook

// Crop and rotate a still picture (bar button; Esc cancels): the picture fitted,
// turned in quarter turns, with a frame to drag (corners and edges; inside moves
// it). A ratio can be fixed (then the corners keep it). Save Copy writes
// “photo (edited).jpg” next to it, Replace puts the result in its place and the
// original in the Trash; metadata is kept (ImageEditor).
Rectangle {
    id: editorView
    objectName: "pictureEditor"

    // Quarter turns counterclockwise, and the part kept as fractions of the turned picture.
    property int turn: 0
    property real cropX: 0
    property real cropY: 0
    property real cropW: 1
    property real cropH: 1
    // Fixed width : height of the part, in picture pixels; 0: free.
    property real ratio: 0

    readonly property size pictureSize: Viewer.imageSize // as shown (EXIF orientation applied)
    readonly property bool sideways: turn % 180 !== 0
    readonly property real turnedWidth: sideways ? pictureSize.height : pictureSize.width
    readonly property real turnedHeight: sideways ? pictureSize.width : pictureSize.height
    readonly property bool changed: turn !== 0 || cropX > 0.0005 || cropY > 0.0005 || cropW < 0.9995 || cropH < 0.9995
    readonly property int pixelWidth: Math.round(cropW * turnedWidth)
    readonly property int pixelHeight: Math.round(cropH * turnedHeight)

    // A quarter turn left (+90, counterclockwise) or right (-90); the frame turns along.
    function rotate(by) {
        const x = cropX, y = cropY, w = cropW, h = cropH;
        if (by > 0) {
            cropX = y; cropY = 1 - x - w;
        } else {
            cropX = 1 - y - h; cropY = x;
        }
        cropW = h;
        cropH = w;
        turn = (turn + by + 360) % 360;
        if (ratio > 0) {
            ratio = 1 / ratio;
            const turned = ratios.findIndex(entry => Math.abs(entry.value - ratio) < 0.001);
            ratioBox.currentIndex = turned >= 0 ? turned : ratioBox.currentIndex; // 4:3 ↔ 3:4
        }
    }
    // Fixes the ratio: the largest such part around the current one's centre.
    function setRatio(value) {
        ratio = value;
        if (value <= 0) {
            return;
        }
        const centreX = (cropX + cropW / 2) * turnedWidth;
        const centreY = (cropY + cropH / 2) * turnedHeight;
        let w = turnedWidth;
        let h = w / value;
        if (h > turnedHeight) {
            h = turnedHeight;
            w = h * value;
        }
        const x = Math.max(0, Math.min(turnedWidth - w, centreX - w / 2));
        const y = Math.max(0, Math.min(turnedHeight - h, centreY - h / 2));
        cropX = x / turnedWidth;
        cropY = y / turnedHeight;
        cropW = w / turnedWidth;
        cropH = h / turnedHeight;
    }
    function save(replace) {
        imageEditor.save(Viewer.currentUrl, Qt.rect(cropX, cropY, cropW, cropH), turn, replace);
    }
    function notify(text) {
        const window = editorView.QQC2.ApplicationWindow.window;
        if (window) {
            window.showPassiveNotification(text);
        }
    }

    readonly property var ratios: [
        { text: i18nc("@item:inlistbox crop ratio", "Free"), value: 0 },
        { text: i18nc("@item:inlistbox crop ratio of the picture itself", "Original"), value: -1 },
        { text: i18nc("@item:inlistbox crop ratio", "Square"), value: 1 },
        { text: "4:3", value: 4 / 3 },
        { text: "3:4", value: 3 / 4 },
        { text: "16:9", value: 16 / 9 },
        { text: "9:16", value: 9 / 16 }
    ]

    color: Kirigami.Theme.backgroundColor
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false

    ImageEditor {
        id: imageEditor
        // Closing the editor unloads this file: the message first, closing last.
        onSaved: (file, replaced) => {
            editorView.notify(replaced ? i18nc("@info", "Picture replaced. The original is in the Trash.")
                                       : i18nc("@info", "Saved as “%1”.", decodeURIComponent(file.toString().split("/").pop())));
            if (replaced) {
                Viewer.replaceCurrent(file); // shows the new picture and ends the edit: last
            } else {
                Viewer.pictureEditing = false;
            }
        }
        onFailed: message => editorView.notify(message)
    }

    Item {
        id: stage
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: bar.top
        anchors.margins: Kirigami.Units.gridUnit

        // The turned picture, fitted; the frame's coordinates are relative to it.
        Item {
            id: canvas
            objectName: "cropCanvas"

            readonly property real scale: Math.min(stage.width / editorView.turnedWidth, stage.height / editorView.turnedHeight)

            anchors.centerIn: parent
            width: editorView.turnedWidth * scale
            height: editorView.turnedHeight * scale

            Image {
                anchors.centerIn: parent
                width: editorView.pictureSize.width * canvas.scale
                height: editorView.pictureSize.height * canvas.scale
                rotation: -editorView.turn // QML turns clockwise
                source: Viewer.currentUrl
                sourceSize.width: width * Screen.devicePixelRatio
                sourceSize.height: height * Screen.devicePixelRatio
                autoTransform: true
                asynchronous: true
                cache: false
                smooth: true
            }

            // Outside the part: dimmed.
            component Shade: Rectangle {
                color: Qt.rgba(0, 0, 0, 0.55)
            }
            Shade { x: 0; y: 0; width: parent.width; height: frame.y }
            Shade { x: 0; y: frame.y + frame.height; width: parent.width; height: parent.height - y }
            Shade { x: 0; y: frame.y; width: frame.x; height: frame.height }
            Shade { x: frame.x + frame.width; y: frame.y; width: parent.width - x; height: frame.height }

            Rectangle {
                id: frame
                objectName: "cropFrame"
                x: editorView.cropX * canvas.width
                y: editorView.cropY * canvas.height
                width: editorView.cropW * canvas.width
                height: editorView.cropH * canvas.height
                color: "transparent"
                border.color: "white"
                border.width: 1

                // A dark line inside the white one: visible on light pictures too.
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1
                    color: "transparent"
                    border.color: Qt.rgba(0, 0, 0, 0.5)
                    border.width: 1
                }

                // Drag inside: move the part.
                DragHandler {
                    id: move
                    target: null
                    cursorShape: active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                    property rect start
                    onActiveChanged: if (active) start = Qt.rect(frame.x, frame.y, frame.width, frame.height)
                    onActiveTranslationChanged: {
                        if (!active) {
                            return;
                        }
                        const x = Math.max(0, Math.min(canvas.width - start.width, start.x + activeTranslation.x));
                        const y = Math.max(0, Math.min(canvas.height - start.height, start.y + activeTranslation.y));
                        editorView.cropX = x / canvas.width;
                        editorView.cropY = y / canvas.height;
                    }
                }
            }

            // The handles: corners (hx, hy = ±1) and edges (one of them 0; not with a ratio).
            Repeater {
                model: [
                    { hx: -1, hy: -1, cursor: Qt.SizeFDiagCursor }, { hx: 1, hy: -1, cursor: Qt.SizeBDiagCursor },
                    { hx: -1, hy: 1, cursor: Qt.SizeBDiagCursor }, { hx: 1, hy: 1, cursor: Qt.SizeFDiagCursor },
                    { hx: 0, hy: -1, cursor: Qt.SizeVerCursor }, { hx: 0, hy: 1, cursor: Qt.SizeVerCursor },
                    { hx: -1, hy: 0, cursor: Qt.SizeHorCursor }, { hx: 1, hy: 0, cursor: Qt.SizeHorCursor }
                ]

                Rectangle {
                    id: handle
                    required property var modelData
                    readonly property int hx: modelData.hx
                    readonly property int hy: modelData.hy

                    objectName: "cropHandle" + (hx + 1) + (hy + 1)
                    visible: editorView.ratio === 0 || (hx !== 0 && hy !== 0)
                    width: Kirigami.Units.gridUnit * 0.75
                    height: width
                    x: frame.x + (hx + 1) / 2 * frame.width - width / 2
                    y: frame.y + (hy + 1) / 2 * frame.height - height / 2
                    color: "white"
                    border.color: Qt.rgba(0, 0, 0, 0.6)
                    radius: 2

                    // A larger area to grab than to see.
                    Item {
                        anchors.centerIn: parent
                        width: Kirigami.Units.gridUnit * 1.5
                        height: width

                        HoverHandler {
                            cursorShape: handle.modelData.cursor
                        }
                        DragHandler {
                            target: null
                            property rect start
                            onActiveChanged: if (active) start = Qt.rect(frame.x, frame.y, frame.width, frame.height)
                            onActiveTranslationChanged: if (active) resize(start, activeTranslation.x, activeTranslation.y)

                            // New edges from the dragged ones, kept inside the picture and
                            // at least a few picture pixels big; with a ratio, kept too.
                            function resize(s, dx, dy) {
                                const minimum = Math.max(8, 16 * canvas.scale);
                                let left = s.x, top = s.y, right = s.x + s.width, bottom = s.y + s.height;
                                if (editorView.ratio > 0) {
                                    // From the opposite corner, which stays.
                                    const anchorX = handle.hx < 0 ? right : left;
                                    const anchorY = handle.hy < 0 ? bottom : top;
                                    const r = editorView.ratio;
                                    let w = Math.abs((handle.hx < 0 ? left + dx : right + dx) - anchorX);
                                    let h = Math.abs((handle.hy < 0 ? top + dy : bottom + dy) - anchorY);
                                    if (w / h > r) {
                                        w = h * r;
                                    } else {
                                        h = w / r;
                                    }
                                    const maxW = handle.hx < 0 ? anchorX : canvas.width - anchorX;
                                    const maxH = handle.hy < 0 ? anchorY : canvas.height - anchorY;
                                    if (w > maxW) { w = maxW; h = w / r; }
                                    if (h > maxH) { h = maxH; w = h * r; }
                                    if (w < minimum || h < minimum) {
                                        w = Math.max(minimum, minimum * r);
                                        h = w / r;
                                    }
                                    left = handle.hx < 0 ? anchorX - w : anchorX;
                                    top = handle.hy < 0 ? anchorY - h : anchorY;
                                    right = left + w;
                                    bottom = top + h;
                                } else {
                                    if (handle.hx < 0) left = Math.max(0, Math.min(right - minimum, left + dx));
                                    if (handle.hx > 0) right = Math.min(canvas.width, Math.max(left + minimum, right + dx));
                                    if (handle.hy < 0) top = Math.max(0, Math.min(bottom - minimum, top + dy));
                                    if (handle.hy > 0) bottom = Math.min(canvas.height, Math.max(top + minimum, bottom + dy));
                                }
                                editorView.cropX = left / canvas.width;
                                editorView.cropY = top / canvas.height;
                                editorView.cropW = (right - left) / canvas.width;
                                editorView.cropH = (bottom - top) / canvas.height;
                            }
                        }
                    }
                }
            }
        }
    }

    QQC2.ToolBar {
        id: bar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        position: QQC2.ToolBar.Footer
        Kirigami.Theme.colorSet: Kirigami.Theme.Window

        RowLayout {
            anchors.fill: parent
            spacing: Kirigami.Units.smallSpacing

            QQC2.ToolButton {
                objectName: "pictureRotateLeft"
                display: QQC2.AbstractButton.IconOnly
                icon.name: "object-rotate-left"
                text: i18nc("@action:button", "Rotate Left")
                enabled: !imageEditor.busy
                onClicked: editorView.rotate(90)
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
            QQC2.ToolButton {
                objectName: "pictureRotateRight"
                display: QQC2.AbstractButton.IconOnly
                icon.name: "object-rotate-right"
                text: i18nc("@action:button", "Rotate Right")
                enabled: !imageEditor.busy
                onClicked: editorView.rotate(-90)
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
            QQC2.ComboBox {
                id: ratioBox
                objectName: "cropRatio"
                model: editorView.ratios
                textRole: "text"
                enabled: !imageEditor.busy
                onActivated: index => {
                    const value = editorView.ratios[index].value;
                    editorView.setRatio(value < 0 ? editorView.turnedWidth / editorView.turnedHeight : value);
                }
            }
            QQC2.Label {
                objectName: "cropSize"
                color: Kirigami.Theme.disabledTextColor
                font.features: { "tnum": 1 }
                text: i18nc("@info picture size in pixels, width × height", "%1 × %2", editorView.pixelWidth, editorView.pixelHeight)
            }
            Item {
                Layout.fillWidth: true
            }
            QQC2.BusyIndicator {
                Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                Layout.preferredWidth: Layout.preferredHeight
                visible: imageEditor.busy
                running: visible
            }
            QQC2.Button {
                objectName: "pictureSaveCopy"
                icon.name: "document-save-as"
                text: i18nc("@action:button", "Save Copy")
                enabled: editorView.changed && !imageEditor.busy
                onClicked: editorView.save(false)
                QQC2.ToolTip.text: i18nc("@info:tooltip", "Saves the result as a new picture next to this one; this one stays as it is.")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
            QQC2.Button {
                objectName: "pictureReplace"
                icon.name: "document-save"
                text: i18nc("@action:button", "Replace")
                enabled: editorView.changed && !imageEditor.busy
                onClicked: editorView.save(true)
                QQC2.ToolTip.text: i18nc("@info:tooltip", "Puts the result in place of this picture. The original goes to the Trash, so it can be restored.")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
            QQC2.ToolButton {
                display: QQC2.AbstractButton.IconOnly
                icon.name: "dialog-cancel"
                text: i18nc("@action:button stop editing the picture", "Cancel")
                enabled: !imageEditor.busy
                onClicked: Viewer.pictureEditing = false
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
        }
    }
}
