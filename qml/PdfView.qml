// SPDX-License-Identifier: GPL-2.0-or-later
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
// The sidebar (bar button) on the left: “Pages”, small pages (PDFs with several), and
// “Contents”, the PDF's bookmarks (also office documents' headings); the current one
// marked, a click goes there. Tabs when both are there.
Item {
    id: view
    objectName: "pdfView"

    // The user zoomed (for the zoom indicator).
    signal zoomed()

    // Opened (Main fades from the previous file's view then).
    readonly property bool ready: pdf.status === PdfDocument.Ready || pdf.status === PdfDocument.Error

    // Page width as a share of the fitted width.
    property real zoom: 1
    readonly property int pageCount: pdf.status === PdfDocument.Ready ? pdf.pageCount : 0
    // The page a third down the view (for the overview).
    readonly property int currentPage: {
        pages.contentY; // follows scrolling
        const i = pages.indexAt(pages.contentX + pages.width / 2, pages.contentY + pages.height / 3);
        return Math.max(0, i);
    }

    function showPage(index) {
        pages.positionViewAtIndex(index, ListView.Beginning);
    }

    // The bookmarks, flattened: [{title, level, page, y}] (y in points from the page's top).
    readonly property var contents: bookmarks.entries
    readonly property bool sidebarAvailable: pageCount > 1 || contents.length > 0
    // The tab shown: the one chosen last, if this PDF has it.
    readonly property string sidebarTab: contents.length > 0 && (Viewer.sidebarTab === "contents" || pageCount <= 1) ? "contents" : "pages"
    // The last bookmark at or above the top of the view (at the end: the last one on
    // screen, as the last ones can't scroll up to the top); the first one until a
    // bookmark has scrolled past.
    readonly property int currentContentsEntry: {
        pages.contentY; // follows scrolling
        pages.contentHeight; // and zooming
        if (contents.length === 0 || pages.count === 0) {
            return -1;
        }
        const top = pages.atYEnd ? pages.contentY + pages.height - Kirigami.Units.gridUnit
            : pages.contentY + Kirigami.Units.gridUnit; // a heading just scrolled to counts
        let page = pages.indexAt(pages.contentX + pages.width / 2, top);
        if (page < 0) {
            page = pages.indexAt(pages.contentX + pages.width / 2, top + pages.spacing); // between pages
        }
        const row = page >= 0 ? pages.itemAtIndex(page) : null;
        if (!row || row.pointSize.width <= 0) {
            return 0;
        }
        const y = (top - row.y) / (pages.pageWidth / row.pointSize.width); // in points
        let found = 0;
        for (let i = 0; i < contents.length; ++i) {
            const entry = contents[i];
            if (entry.page < page || (entry.page === page && entry.y <= y)) {
                found = i;
            }
        }
        return found;
    }
    // Scrolls `page` to the top, with the spot `y` points down it near the top edge.
    function showDestination(page, y) {
        pages.positionViewAtIndex(page, ListView.Beginning);
        const points = pdf.pagePointSize(page);
        if (y > 0 && points.width > 0) {
            const scale = pages.pageWidth / points.width;
            pages.contentY = Math.max(pages.originY, Math.min(pages.originY + pages.contentHeight - pages.height,
                pages.contentY + y * scale - Kirigami.Units.largeSpacing));
        }
    }
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
        if (search && search.matchCount > 0) {
            showResult((search.currentResult + 1) % search.matchCount);
        }
    }
    function findPrevious() {
        if (search && search.matchCount > 0) {
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
    PdfContents {
        id: bookmarks
    }
    // Finds page by page in the background; the first match shows as soon as it's found.
    // QML doesn't get the model's count and status properties (newer than its QML
    // registration): the count comes from the model's rows.
    // Made only while the find bar is open: an idle search model still wakes the viewer
    // ten times a second (measured 2026-11-23), also while the preview is hidden.
    readonly property PdfSearchModel search: searchLoader.item
    Loader {
        id: searchLoader
        active: Viewer.searchOpen
        sourceComponent: PdfSearchModel {
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
            searchString: Viewer.searchText
            onSearchStringChanged: showFirst = true
            onRowsInserted: updateCount()
            onRowsRemoved: updateCount()
            onModelReset: updateCount()
        }
    }
    // The PDF itself, or an office document converted to one; only once the file's
    // details say so (not the previous file's type). Kept, not cleared, when the next
    // file has none: an empty source logs a warning. Set here rather than by a Binding
    // with `when`: its value could update before its condition, setting an empty
    // source for a moment when going from one PDF to the next.
    function updateSource() {
        if (Viewer.pdfUrl.toString() !== "" && pdf.source !== Viewer.pdfUrl) {
            // Thumbnails of the old PDF go first: a kept one would show the old page.
            thumbnailsLoader.active = false;
            pdf.source = Viewer.pdfUrl;
            bookmarks.source = Viewer.pdfUrl;
            thumbnailsLoader.active = Qt.binding(() => sidebar.visible && view.sidebarTab === "pages");
        }
    }
    Component.onCompleted: updateSource()
    Connections {
        target: Viewer
        function onFileInfoChanged() {
            view.updateSource();
        }
    }

    // The sidebar: page overview or contents.
    Item {
        id: sidebar
        objectName: "pageOverview"

        // One width for both tabs: nothing jumps when switching, and both names fit.
        readonly property real fullWidth: Kirigami.Units.gridUnit * 12

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: scroll.bottom
        width: Viewer.pagesOpen && view.sidebarAvailable ? fullWidth : 0
        visible: width > 0
        clip: true
        Behavior on width {
            NumberAnimation {
                duration: Kirigami.Units.longDuration
                easing.type: Easing.InOutQuad
            }
        }
        Kirigami.Theme.colorSet: Kirigami.Theme.Window
        Kirigami.Theme.inherit: false

        Rectangle {
            anchors.fill: parent
            color: Kirigami.Theme.backgroundColor
        }

        QQC2.TabBar {
            id: tabs
            objectName: "sidebarTabs"
            width: sidebar.fullWidth
            visible: view.pageCount > 1 && view.contents.length > 0
            currentIndex: view.sidebarTab === "contents" ? 1 : 0

            // The two tabs share the bar (on their own they keep their text's width).
            QQC2.TabButton {
                objectName: "pagesTab"
                width: tabs.width / 2
                text: i18nc("@title:tab small pages of a PDF", "Pages")
                onClicked: Viewer.sidebarTab = "pages"
            }
            QQC2.TabButton {
                objectName: "contentsTab"
                width: tabs.width / 2
                text: i18nc("@title:tab headings of a document", "Contents")
                onClicked: Viewer.sidebarTab = "contents"
            }
        }

        // One tab only: its name as the heading.
        PanelHeading {
            id: sidebarHeading
            width: sidebar.fullWidth
            height: tabs.implicitHeight
            visible: !tabs.visible
            text: view.sidebarTab === "contents" ? i18nc("@title headings of a document", "Contents") : i18nc("@title small pages of a PDF", "Pages")
        }

        // Only while shown: no rendering for a closed overview.
        Loader {
            id: thumbnailsLoader
            // Full width while sliding, so the pages don't resize.
            width: sidebar.fullWidth
            anchors.top: tabs.visible ? tabs.bottom : sidebarHeading.bottom
            anchors.bottom: parent.bottom
            active: sidebar.visible && view.sidebarTab === "pages"
            sourceComponent: thumbnailsComponent
        }
        Loader {
            width: sidebar.fullWidth
            anchors.top: tabs.visible ? tabs.bottom : sidebarHeading.bottom
            anchors.bottom: parent.bottom
            active: sidebar.visible && view.sidebarTab === "contents"
            sourceComponent: ContentsList {
                entries: view.contents
                currentIndex: view.currentContentsEntry
                onActivated: index => view.showDestination(view.contents[index].page, view.contents[index].y)
            }
        }
        Component {
            id: thumbnailsComponent

            QQC2.ScrollView {
                id: thumbnailScroll
                QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff
                Component.onCompleted: thumbnails.positionViewAtIndex(view.currentPage, ListView.Contain)

                ListView {
                    id: thumbnails
                    model: view.pageCount
                    spacing: Kirigami.Units.largeSpacing
                    topMargin: Kirigami.Units.largeSpacing
                    bottomMargin: Kirigami.Units.largeSpacing

                    delegate: Item {
                        id: thumb

                        required property int index
                        readonly property size pointSize: pdf.pagePointSize(index)
                        readonly property bool current: view.currentPage === index

                        width: ListView.view.width
                        height: paper.height + number.height + Kirigami.Units.smallSpacing

                        Rectangle {
                            id: paper
                            objectName: "pageThumbnail"
                            anchors.horizontalCenter: parent.horizontalCenter
                            // As if the scrollbar were always there: from the list's width, the
                            // pages' height would make the scrollbar come and go, and the list
                            // narrow and widen with it, re-rendering the pages each time.
                            width: thumbnailScroll.width - thumbnailScroll.QQC2.ScrollBar.vertical.width - Kirigami.Units.largeSpacing * 3
                            height: thumb.pointSize.width > 0 ? width * thumb.pointSize.height / thumb.pointSize.width : width * 1.414
                            color: "white"
                            border.width: thumb.current ? 2 : 0
                            border.color: Kirigami.Theme.highlightColor

                            PdfPageImage {
                                anchors.fill: parent
                                anchors.margins: parent.border.width
                                document: pdf
                                currentFrame: thumb.index
                                asynchronous: true
                                sourceSize.width: width * Screen.devicePixelRatio
                                sourceSize.height: height * Screen.devicePixelRatio
                            }
                        }
                        QQC2.Label {
                            id: number
                            anchors.top: paper.bottom
                            anchors.topMargin: Kirigami.Units.smallSpacing
                            anchors.horizontalCenter: parent.horizontalCenter
                            font: Kirigami.Theme.smallFont
                            color: thumb.current ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                            text: thumb.index + 1
                        }
                        TapHandler {
                            onTapped: view.showPage(thumb.index)
                        }
                    }
                }
                // The overview keeps the current page in sight.
                Connections {
                    target: view
                    function onCurrentPageChanged() {
                        thumbnails.positionViewAtIndex(view.currentPage, ListView.Contain);
                    }
                }
            }
        }
        Kirigami.Separator {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
        }
    }

    QQC2.ScrollView {
        id: scroll
        anchors.left: sidebar.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: findBar.visible ? findBar.top : parent.bottom
        visible: pdf.status === PdfDocument.Ready
        // Sideways only when zoomed in beyond the width.
        QQC2.ScrollBar.horizontal.policy: view.zoom > 1 ? QQC2.ScrollBar.AsNeeded : QQC2.ScrollBar.AlwaysOff

        ListView {
            id: pages
            objectName: "pdfPages"

            readonly property real fittedWidth: Math.max(Kirigami.Units.gridUnit * 4, scroll.availableWidth - Kirigami.Units.gridUnit * 2)
            readonly property real pageWidth: fittedWidth * view.zoom
            // The width pages are rendered at: follows pageWidth once it has settled. Each
            // change re-renders every page and cancels renders still running, and Qt 6.12
            // can crash on a cancel at the wrong moment (see CLAUDE.md, Known Pitfalls).
            // Meanwhile the last rendering is scaled.
            property real renderWidth: 0
            onPageWidthChanged: renderTimer.restart()
            Timer {
                id: renderTimer
                interval: 150
                onTriggered: pages.renderWidth = pages.pageWidth
            }

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

                    // Not before the width has settled: a first rendering at a passing
                    // width would be cancelled.
                    Loader {
                        anchors.fill: parent
                        active: pages.renderWidth > 0
                        sourceComponent: PdfPageImage {
                            document: pdf
                            currentFrame: row.index
                            asynchronous: true
                            sourceSize.width: pages.renderWidth * Screen.devicePixelRatio
                            sourceSize.height: pages.renderWidth * page.height / page.width * Screen.devicePixelRatio
                        }
                    }

                    // Matches on this page (in page points), the current one stronger.
                    Shape {
                        anchors.fill: parent
                        visible: view.search !== null && view.search.matchCount > 0

                        ShapePath {
                            strokeWidth: -1
                            fillColor: Qt.alpha(Kirigami.Theme.highlightColor, 0.3)
                            scale: Qt.size(page.width / row.pointSize.width, page.width / row.pointSize.width)
                            PathMultiline {
                                id: pageMatches
                                objectName: "pdfMatches"
                                function update() {
                                    paths = view.search ? view.search.boundingPolygonsOnPage(row.index) : [];
                                }
                                Component.onCompleted: update()
                            }
                        }
                        ShapePath {
                            strokeWidth: -1
                            fillColor: Qt.alpha(Kirigami.Theme.highlightColor, 0.5)
                            scale: Qt.size(page.width / row.pointSize.width, page.width / row.pointSize.width)
                            PathMultiline {
                                paths: view.search && view.search.currentPage === row.index ? view.search.currentResultBoundingPolygons : []
                            }
                        }
                    }
                    // As Qt's PDF views do: when the matches change, every page asks again.
                    Connections {
                        target: view.search
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

    // “3 / 12” while scrolling (not when a document opens).
    FloatingLabel {
        id: pageNumber
        objectName: "pageNumber"
        anchors.horizontalCenter: scroll.horizontalCenter
        anchors.bottom: scroll.bottom
        anchors.bottomMargin: Kirigami.Units.largeSpacing * 2
        text: i18nc("@info page number while scrolling, e.g. 3 / 12", "%1 / %2", view.currentPage + 1, view.pageCount)

        Timer {
            id: openedTimer // a new document scrolls to its start by itself
            interval: 500
        }
        Connections {
            target: pdf
            function onStatusChanged() {
                openedTimer.restart();
            }
        }
        Connections {
            target: pages
            function onContentYChanged() {
                if (view.pageCount > 1 && !openedTimer.running) {
                    pageNumber.flash();
                }
            }
        }
    }

    // The find bar's height, for floating notes in the corner to keep above it.
    readonly property real bottomBarHeight: findBar.visible ? findBar.height : 0

    FindBar {
        id: findBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        count: view.search ? view.search.matchCount : 0
        current: view.search ? view.search.currentResult : -1
        onNext: view.findNext()
        onPrevious: view.findPrevious()
    }

    Loader {
        anchors.fill: parent
        active: pdf.status === PdfDocument.Error
        sourceComponent: FallbackView {}
    }
}
