// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Window
import org.kde.kirigami as Kirigami
import kwiklook

// The index sheet: every file of the preview as a tile (KDE's thumbnail or the type
// icon, and the name), like Quick Look's. Arrow keys move, Enter or a click shows that
// file; Esc (Main.qml) and Ctrl+Enter go back. Only tiles on screen ask for thumbnails.
QQC2.ScrollView {
    id: sheet
    objectName: "indexSheet"

    function focusGrid() {
        grid.currentIndex = Viewer.currentIndex;
        grid.positionViewAtIndex(grid.currentIndex, GridView.Contain);
        grid.forceActiveFocus();
    }

    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false
    QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

    background: Rectangle {
        color: Kirigami.Theme.backgroundColor
    }

    GridView {
        id: grid
        objectName: "indexGrid"

        readonly property int thumbnailSize: Kirigami.Units.iconSizes.enormous

        // Columns fill the width; tiles keep their size.
        cellWidth: Math.max(thumbnailSize + Kirigami.Units.gridUnit, Math.floor(width / Math.max(1, Math.floor(width / (thumbnailSize + Kirigami.Units.gridUnit * 2)))))
        cellHeight: thumbnailSize + Kirigami.Units.gridUnit * 4
        topMargin: Kirigami.Units.largeSpacing
        bottomMargin: Kirigami.Units.largeSpacing
        model: Viewer.count
        keyNavigationEnabled: true
        highlightMoveDuration: 0
        Keys.onReturnPressed: Viewer.showFile(currentIndex)
        Keys.onEnterPressed: Viewer.showFile(currentIndex)

        // The KDE style's own highlight (current) and hover, as Dolphin's icons.
        delegate: QQC2.ItemDelegate {
            id: tile

            required property int index
            readonly property var file: Viewer.fileAt(index)

            objectName: "indexTile"
            width: GridView.view.cellWidth
            height: GridView.view.cellHeight
            leftInset: Kirigami.Units.smallSpacing
            rightInset: Kirigami.Units.smallSpacing
            topInset: Kirigami.Units.smallSpacing
            bottomInset: Kirigami.Units.smallSpacing
            padding: Kirigami.Units.largeSpacing
            highlighted: GridView.isCurrentItem
            focusPolicy: Qt.NoFocus // the grid keeps the keys
            onClicked: Viewer.showFile(index)

            contentItem: Item {
                Image {
                    id: thumbnail
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: grid.thumbnailSize
                    height: width
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    sourceSize.width: width * Screen.devicePixelRatio
                    sourceSize.height: height * Screen.devicePixelRatio
                    source: "image://thumbnail/" + encodeURIComponent(tile.file.mimeType) + "/" + encodeURIComponent(tile.file.url.toString())
                    visible: status === Image.Ready && implicitWidth > 1 // 1×1: no thumbnail
                }
                Kirigami.Icon {
                    anchors.fill: thumbnail
                    source: tile.file.iconName
                    fallback: "unknown"
                    selected: tile.highlighted
                    visible: !thumbnail.visible
                }
                QQC2.Label {
                    anchors.top: thumbnail.bottom
                    anchors.topMargin: Kirigami.Units.smallSpacing
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideMiddle
                    textFormat: Text.PlainText
                    color: tile.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                    text: tile.file.name
                }
            }
        }
    }
}
