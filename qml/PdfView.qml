// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Pdf
import QtQuick.Window
import org.kde.kirigami as Kirigami
import kwiklook

// PDFs: all pages one below the other, fitted to the width; scroll through them.
// Ctrl+wheel or pinch zooms towards the pointer; each PDF starts fitted.
// A list of PdfPageImage in KDE's ScrollView rather than Qt's PdfMultiPageView,
// which always scrolls sideways and puts pages off-centre (its rows are wider than
// the view). A PDF that can't be opened (damaged, password-protected) shows the
// fallback view.
Item {
    id: view
    objectName: "pdfView"

    // The user zoomed (for the zoom indicator).
    signal zoomed()

    // Page width as a share of the fitted width.
    property real zoom: 1
    // As a share of the page's real size (first page, at 96 dpi), as Okular shows it.
    readonly property real zoomPercent: {
        const points = pdf.status === PdfDocument.Ready ? pdf.pagePointSize(0).width : 0;
        return points > 0 ? Math.round(pages.pageWidth / (points * 96 / 72) * 100) : Math.round(zoom * 100);
    }

    // Zoom by `factor`, keeping the spot under `point` (view coordinates) in place.
    function zoomBy(factor, point) {
        const old = zoom;
        let z = Math.max(0.25, Math.min(6, old * factor));
        // Stop at the fitted width on the way through.
        if ((old < 1 && z > 1) || (old > 1 && z < 1)) {
            z = 1;
        }
        if (Math.abs(z - old) < 0.0001) {
            return;
        }
        const p = point !== undefined ? point : Qt.point(pages.width / 2, pages.height / 2);
        const r = pages.pageWidth;
        const docX = pages.contentX - pages.originX + p.x;
        const docY = pages.contentY - pages.originY + p.y;
        zoom = z;
        const ratio = pages.pageWidth / r;
        // After the list has laid out the resized pages.
        Qt.callLater(() => {
            pages.contentX = Math.max(pages.originX, Math.min(pages.originX + pages.contentWidth - pages.width, pages.originX + docX * ratio - p.x));
            pages.contentY = Math.max(pages.originY, Math.min(pages.originY + pages.contentHeight - pages.height, pages.originY + docY * ratio - p.y));
        });
        zoomed();
    }
    function resetZoom() {
        zoom = 1;
        zoomed();
    }

    Connections {
        target: Viewer
        function onCurrentChanged() {
            view.zoom = 1;
        }
    }

    PdfDocument {
        id: pdf // not “document”: PdfPageImage.document would bind to itself
    }
    // Only once the file's details say it is a PDF (not the previous file's type).
    // Kept, not cleared, when the next file isn't one: an empty source logs a warning.
    Binding {
        target: pdf
        property: "source"
        when: Viewer.mimeType === "application/pdf"
        value: Viewer.currentUrl
        restoreMode: Binding.RestoreNone
    }

    QQC2.ScrollView {
        id: scroll
        anchors.fill: parent
        visible: pdf.status === PdfDocument.Ready
        // Sideways only when zoomed in beyond the width.
        QQC2.ScrollBar.horizontal.policy: view.zoom > 1 ? QQC2.ScrollBar.AsNeeded : QQC2.ScrollBar.AlwaysOff

        ListView {
            id: pages

            readonly property real fittedWidth: Math.max(Kirigami.Units.gridUnit * 4, scroll.availableWidth - Kirigami.Units.gridUnit * 2)
            readonly property real pageWidth: fittedWidth * view.zoom

            model: pdf.status === PdfDocument.Ready ? pdf.pageCount : 0
            spacing: Kirigami.Units.largeSpacing
            topMargin: Kirigami.Units.largeSpacing
            bottomMargin: Kirigami.Units.largeSpacing
            cacheBuffer: height // render the next page before it scrolls in
            contentWidth: Math.max(width, pageWidth + Kirigami.Units.gridUnit * 2)
            flickableDirection: Flickable.AutoFlickDirection

            // In the list, which would otherwise take the wheel itself. Positions via the
            // scene: the list may give its handlers to its content item.
            WheelHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad // Wayland types the mouse as a touchpad
                acceptedModifiers: Qt.ControlModifier
                onWheel: event => view.zoomBy(Math.pow(1.25, event.angleDelta.y / 120), pages.mapFromItem(null, point.scenePosition))
            }
            PinchHandler {
                target: null
                onScaleChanged: delta => view.zoomBy(delta, pages.mapFromItem(null, centroid.scenePosition))
            }

            // A row as wide as the content with the page centred in it: the list sets
            // its delegates' x itself (back to 0 when it lays them out again), which
            // would undo a binding on x.
            delegate: Item {
                id: row

                required property int index
                readonly property size pointSize: pdf.pagePointSize(index)

                width: pages.contentWidth
                height: page.height

                Rectangle {
                    id: page
                    objectName: "pdfPage"
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: pages.pageWidth
                    height: row.pointSize.width > 0 ? width * row.pointSize.height / row.pointSize.width : width * 1.414
                    color: "white" // paper, also while the page renders

                    PdfPageImage {
                        anchors.fill: parent
                        document: pdf
                        currentFrame: row.index
                        asynchronous: true
                        sourceSize.width: page.width * Screen.devicePixelRatio
                        sourceSize.height: page.height * Screen.devicePixelRatio
                    }
                }
            }
        }
    }

    Loader {
        anchors.fill: parent
        active: pdf.status === PdfDocument.Error
        sourceComponent: FallbackView {}
    }
}
