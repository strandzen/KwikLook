// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Window
import org.kde.kirigami as Kirigami
import kwiklook

// Pictures: as large as fits, never enlarged beyond their real size (1 image pixel
// = 1 screen pixel), so small ones stay sharp. Name and details are in the bar and
// the information panel. A picture that can't be decoded shows the fallback view.
Item {
    id: view
    objectName: "imageView"

    readonly property real dpr: Screen.devicePixelRatio
    // Real size in logical pixels (from the file's details, which also tell that the
    // current file is a picture: until they arrive, nothing is loaded).
    readonly property real naturalWidth: Viewer.imageSize.width / dpr
    readonly property real naturalHeight: Viewer.imageSize.height / dpr
    readonly property real fit: naturalWidth > 0 && naturalHeight > 0
        ? Math.min(1, (width - Kirigami.Units.largeSpacing * 2) / naturalWidth, (height - Kirigami.Units.largeSpacing * 2) / naturalHeight)
        : 0
    // Pictures larger than the screen are decoded at screen size: same look, far less memory.
    readonly property bool largerThanScreen: Viewer.imageSize.width > Screen.width * dpr || Viewer.imageSize.height > Screen.height * dpr

    Image {
        id: image
        anchors.centerIn: parent
        width: view.naturalWidth * view.fit
        height: view.naturalHeight * view.fit
        source: Viewer.imageSize.width > 0 ? Viewer.currentUrl : ""
        asynchronous: true
        autoTransform: true // EXIF orientation
        cache: false
        smooth: true
        mipmap: true // clean when shrunk a lot
        fillMode: Image.PreserveAspectFit
        sourceSize: view.largerThanScreen ? Qt.size(Screen.width * view.dpr, Screen.height * view.dpr) : undefined
    }

    QQC2.BusyIndicator {
        anchors.centerIn: parent
        running: image.status === Image.Loading
        visible: running
    }

    // Created only when needed: it would ask for a thumbnail of every picture.
    Loader {
        anchors.fill: parent
        // Can't be decoded, or its size can't even be read (damaged file).
        active: image.status === Image.Error || (Viewer.mimeType !== "" && Viewer.imageSize.width <= 0)
        sourceComponent: FallbackView {}
    }
}
