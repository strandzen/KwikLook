// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Window
import org.kde.kirigami as Kirigami
import kwiklook

// The current file's thumbnail as Dolphin shows it (KDE's thumbnailers; an audio
// file's cover), or its type icon until there is one and when there is none.
Item {
    id: root

    // Width and height (the thumbnail keeps its proportions inside).
    property int size: Kirigami.Units.iconSizes.enormous
    // Off: the type icon only (no thumbnail is asked for).
    property bool thumbnailEnabled: true
    // Shown when the type has no icon of its own.
    property string fallbackIcon: "unknown"

    // The thumbnail shown (audio: the cover), for a blurred copy behind it.
    readonly property bool hasThumbnail: thumbnail.visible
    readonly property Item thumbnailItem: thumbnail

    implicitWidth: size
    implicitHeight: size

    Image {
        id: thumbnail
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        cache: false
        sourceSize.width: root.size * Screen.devicePixelRatio
        sourceSize.height: root.size * Screen.devicePixelRatio
        source: Viewer.fileExists && root.thumbnailEnabled
            ? "image://thumbnail/" + encodeURIComponent(Viewer.mimeType) + "/" + encodeURIComponent(Viewer.currentUrl.toString())
            : ""
        visible: status === Image.Ready && implicitWidth > 1 // 1×1: no thumbnail (ThumbnailProvider)
    }

    Kirigami.Icon {
        anchors.centerIn: parent
        width: Math.min(parent.width, Kirigami.Units.iconSizes.enormous * 2)
        height: width
        source: Viewer.iconName
        fallback: root.fallbackIcon
        visible: !thumbnail.visible && Viewer.iconName !== ""
    }
}
