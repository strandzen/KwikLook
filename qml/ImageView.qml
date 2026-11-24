// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Window
import org.kde.kirigami as Kirigami
import kwiklook

// Pictures: as large as fits, never enlarged beyond their real size (1 image pixel
// = 1 screen pixel), so small ones stay sharp. Name and details are in the bar and
// the information panel. Animated ones (GIF, WebP, ...) play. A picture that can't
// be decoded shows the fallback view.
//
// Zoom like Gwenview: Ctrl+wheel or pinch towards the pointer, double-click for real
// size and back; a picture larger than the view is moved by dragging or scrolling.
// Each picture starts fitted.
Item {
    id: view
    objectName: "imageView"

    // The user zoomed (for the zoom indicator).
    signal zoomed()
    // Decoded (Main fades from the previous file's view then).
    readonly property bool ready: image.status !== Image.Loading

    readonly property real dpr: Screen.devicePixelRatio
    // Real size in logical pixels (from the file's details, which also tell that the
    // current file is a picture: until they arrive, nothing is loaded).
    readonly property real naturalWidth: Viewer.imageSize.width / dpr
    readonly property real naturalHeight: Viewer.imageSize.height / dpr
    readonly property real fit: naturalWidth > 0 && naturalHeight > 0
        ? Math.min(1, (width - Kirigami.Units.largeSpacing * 2) / naturalWidth, (height - Kirigami.Units.largeSpacing * 2) / naturalHeight)
        : 0
    // 0: fitted (follows the window size); otherwise the chosen scale, 1 = real size.
    property real userZoom: 0
    readonly property real zoom: userZoom > 0 ? userZoom : fit
    readonly property real zoomPercent: Math.round(zoom * 100)
    readonly property real maxZoom: 8
    // Where the picture's centre is, from the view's centre (when larger than the view).
    property real panX: 0
    property real panY: 0
    readonly property real maxPanX: Math.max(0, (naturalWidth * zoom - width) / 2)
    readonly property real maxPanY: Math.max(0, (naturalHeight * zoom - height) / 2)
    readonly property bool pannable: maxPanX > 0 || maxPanY > 0

    // Pictures larger than the screen are decoded at screen size while fitted: same
    // look, far less memory. Zoomed in, the full picture is decoded.
    readonly property bool largerThanScreen: Viewer.imageSize.width > Screen.width * dpr || Viewer.imageSize.height > Screen.height * dpr
    // The one showing the current picture.
    readonly property Image image: Viewer.imageAnimated ? animatedImage : stillImage

    function clamp(value, low, high) {
        return Math.max(low, Math.min(high, value));
    }

    // Zoom to `target`, keeping the picture point under `point` (view coordinates) in place.
    function zoomTo(target, point) {
        if (fit <= 0) {
            return;
        }
        const old = zoom;
        let z = clamp(target, fit, maxZoom);
        // Stop at real size on the way through, as Gwenview does.
        if ((old < 1 && z > 1) || (old > 1 && z < 1)) {
            z = 1;
        }
        if (Math.abs(z - old) < 0.0001) {
            return;
        }
        const p = point !== undefined ? point : Qt.point(width / 2, height / 2);
        // The picture's centre moves so that the point under the pointer stays put.
        const centreX = width / 2 + panX;
        const centreY = height / 2 + panY;
        const newCentreX = p.x - (p.x - centreX) * z / old;
        const newCentreY = p.y - (p.y - centreY) * z / old;
        userZoom = z <= fit + 0.0001 ? 0 : z;
        panX = clamp(newCentreX - width / 2, -maxPanX, maxPanX);
        panY = clamp(newCentreY - height / 2, -maxPanY, maxPanY);
        zoomed();
    }
    function zoomBy(factor, point) {
        zoomTo(zoom * factor, point);
    }
    function resetZoom() {
        userZoom = 0;
        panX = 0;
        panY = 0;
        zoomed();
    }
    function panBy(dx, dy) {
        panX = clamp(panX + dx, -maxPanX, maxPanX);
        panY = clamp(panY + dy, -maxPanY, maxPanY);
    }

    // Each picture starts fitted; a smaller window keeps the picture inside its bounds.
    Connections {
        target: Viewer
        function onCurrentChanged() {
            view.userZoom = 0;
            view.panX = 0;
            view.panY = 0;
        }
    }
    onMaxPanXChanged: panX = clamp(panX, -maxPanX, maxPanX)
    onMaxPanYChanged: panY = clamp(panY, -maxPanY, maxPanY)

    // Picture area: both images share it, only the current kind has a source.
    Item {
        id: picture
        x: (view.width - width) / 2 + view.panX
        y: (view.height - height) / 2 + view.panY
        width: view.naturalWidth * view.zoom
        height: view.naturalHeight * view.zoom

        Image {
            id: stillImage
            anchors.fill: parent
            source: Viewer.imageSize.width > 0 && !Viewer.imageAnimated ? Viewer.currentUrl : ""
            asynchronous: true
            retainWhileLoading: true // the screen-size picture stays while the full one decodes
            autoTransform: true // EXIF orientation
            cache: false
            smooth: true
            mipmap: true // clean when shrunk a lot
            fillMode: Image.PreserveAspectFit
            sourceSize: view.largerThanScreen && view.userZoom === 0 ? Qt.size(Screen.width * view.dpr, Screen.height * view.dpr) : undefined
        }

        AnimatedImage {
            id: animatedImage
            objectName: "animatedImage"
            anchors.fill: parent
            source: Viewer.imageSize.width > 0 && Viewer.imageAnimated ? Viewer.currentUrl : ""
            // The view stays loaded while the preview is hidden: don't animate then.
            paused: !(view.Window.window && view.Window.window.visible)
            cache: false
            smooth: true
            mipmap: true
            fillMode: Image.PreserveAspectFit
        }
    }

    // WheelHandler takes only mice by default, and on Wayland Qt reports the mouse
    // as a touchpad: name both, or no wheel event arrives.
    // Ctrl+wheel (and Ctrl+scroll on a touchpad) zooms.
    WheelHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        acceptedModifiers: Qt.ControlModifier
        onWheel: event => view.zoomBy(Math.pow(1.25, event.angleDelta.y / 120), Qt.point(event.x, event.y))
    }
    // Plain scrolling moves a zoomed picture.
    WheelHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        acceptedModifiers: Qt.NoModifier
        enabled: view.pannable
        onWheel: event => {
            // Touchpads (and Wayland wheels) give pixels; otherwise steps of 120.
            const step = Kirigami.Units.gridUnit * 3;
            const dx = event.pixelDelta.x !== 0 ? event.pixelDelta.x : event.angleDelta.x / 120 * step;
            const dy = event.pixelDelta.y !== 0 ? event.pixelDelta.y : event.angleDelta.y / 120 * step;
            view.panBy(dx, dy);
        }
    }
    PinchHandler {
        target: null
        onScaleChanged: delta => view.zoomBy(delta, centroid.position)
    }
    DragHandler {
        id: drag

        property real startX: 0
        property real startY: 0

        target: null
        enabled: view.pannable
        cursorShape: Qt.ClosedHandCursor // while dragging; HoverHandler below otherwise
        onActiveChanged: if (active) {
            startX = view.panX;
            startY = view.panY;
        }
        onActiveTranslationChanged: if (active) {
            view.panX = view.clamp(startX + activeTranslation.x, -view.maxPanX, view.maxPanX);
            view.panY = view.clamp(startY + activeTranslation.y, -view.maxPanY, view.maxPanY);
        }
    }
    HoverHandler {
        enabled: view.pannable
        cursorShape: Qt.OpenHandCursor
    }
    TapHandler {
        onDoubleTapped: eventPoint => {
            if (view.userZoom === 0) {
                view.zoomTo(1, eventPoint.position);
            } else {
                view.resetZoom();
            }
        }
    }

    DelayedBusyIndicator {
        anchors.centerIn: parent
        // Not while a zoomed picture loads in full: the smaller one stays visible.
        busy: view.image.status === Image.Loading && view.userZoom === 0
    }

    // Created only when needed: it would ask for a thumbnail of every picture.
    Loader {
        anchors.fill: parent
        // Can't be decoded, or its size can't even be read (damaged file).
        active: view.image.status === Image.Error || (Viewer.mimeType !== "" && Viewer.imageSize.width <= 0)
        sourceComponent: FallbackView {}
    }
}
