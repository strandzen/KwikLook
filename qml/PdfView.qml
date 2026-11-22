// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Pdf
import QtQuick.Shapes
import QtQuick.Window
import org.kde.kirigami as Kirigami
import kwiklook

// PDFs: all pages one below the other, fitted to the width; scroll through them.
// Ctrl+wheel or pinch zooms towards the pointer; each PDF starts fitted.
// A list of PdfPageImage in KDE's ScrollView rather than Qt's PdfMultiPageView,
// which always scrolls sideways and puts pages off-centre (its rows are wider than
// the view). A PDF that can't be opened (damaged, password-protected) shows the
// fallback view. Ctrl+F finds text (FindBar below); matches are marked on the pages.
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

    function focusSearch() {
        findBar.focusField();
    }
    function findNext() {
        if (search.matchCount > 0) {
            showResult((search.currentResult + 1) % search.matchCount);
        }
    }
    function findPrevious() {
        if (search.matchCount > 0) {
            showResult((search.currentResult - 1 + search.matchCount) % search.matchCount);
        }
    }
    // Makes `index` the current match and scrolls its page there, the match a third down.
    function showResult(index) {
        search.currentResult = index;
        const link = search.currentResultLink;
        const points = pdf.pagePointSize(link.page);
        if (!link.valid || points.width <= 0) {
            return;
        }
        pages.positionViewAtIndex(link.page, ListView.Beginning);
        const scale = pages.pageWidth / points.width;
        pages.contentY = Math.max(pages.originY, Math.min(pages.originY + pages.contentHeight - pages.height,
            pages.contentY + link.location.y * scale - pages.height / 3));
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
    // Finds page by page in the background; the first match shows as soon as it's found.
    // QML doesn't get the model's count and status properties (newer than its QML
    // registration): the count comes from the model's rows.
    PdfSearchModel {
        id: search

        property int matchCount: 0
        property bool showFirst: true

        function updateCount() {
            matchCount = rowCount();
            if (matchCount > 0 && showFirst) {
                showFirst = false;
                view.showResult(0);
            }
        }

        document: pdf
        searchString: Viewer.searchOpen ? Viewer.searchText : ""
        onSearchStringChanged: showFirst = true
        onRowsInserted: updateCount()
        onRowsRemoved: updateCount()
        onModelReset: updateCount()
    }
    // The PDF itself, or an office document converted to one; only once the file's
    // details say so (not the previous file's type). Kept, not cleared, when the next
    // file has none: an empty source logs a warning.
    Binding {
        target: pdf
        property: "source"
        when: Viewer.pdfUrl.toString() !== ""
        value: Viewer.pdfUrl
        restoreMode: Binding.RestoreNone
    }

    QQC2.ScrollView {
        id: scroll
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: findBar.visible ? findBar.top : parent.bottom
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

                    // Matches on this page (in page points), the current one stronger.
                    Shape {
                        anchors.fill: parent
                        visible: search.matchCount > 0

                        ShapePath {
                            strokeWidth: -1
                            fillColor: Qt.alpha(Kirigami.Theme.highlightColor, 0.3)
                            scale: Qt.size(page.width / row.pointSize.width, page.width / row.pointSize.width)
                            PathMultiline {
                                id: pageMatches
                                objectName: "pdfMatches"
                                function update() {
                                    paths = search.boundingPolygonsOnPage(row.index);
                                }
                                Component.onCompleted: update()
                            }
                        }
                        ShapePath {
                            strokeWidth: -1
                            fillColor: Qt.alpha(Kirigami.Theme.highlightColor, 0.5)
                            scale: Qt.size(page.width / row.pointSize.width, page.width / row.pointSize.width)
                            PathMultiline {
                                paths: search.currentPage === row.index ? search.currentResultBoundingPolygons : []
                            }
                        }
                    }
                    // As Qt's PDF views do: when the matches change, every page asks again.
                    Connections {
                        target: search
                        function onCurrentPageBoundingPolygonsChanged() {
                            pageMatches.update();
                        }
                        function onMatchCountChanged() {
                            pageMatches.update();
                        }
                    }
                }
            }
        }
    }

    FindBar {
        id: findBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        count: search.matchCount
        current: search.currentResult
        onNext: view.findNext()
        onPrevious: view.findPrevious()
    }

    Loader {
        anchors.fill: parent
        active: pdf.status === PdfDocument.Error
        sourceComponent: FallbackView {}
    }
}
