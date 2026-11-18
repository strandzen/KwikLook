// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Pdf
import QtQuick.Window
import org.kde.kirigami as Kirigami
import kwiklook

// PDFs: all pages one below the other, fitted to the width; scroll through them.
// A list of PdfPageImage in KDE's ScrollView rather than Qt's PdfMultiPageView,
// which always scrolls sideways and puts pages off-centre (its rows are wider than
// the view). A PDF that can't be opened (damaged, password-protected) shows the
// fallback view.
Item {
    id: view
    objectName: "pdfView"

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
        QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

        ListView {
            id: pages

            readonly property real pageWidth: Math.max(Kirigami.Units.gridUnit * 4, scroll.availableWidth - Kirigami.Units.gridUnit * 2)

            model: pdf.status === PdfDocument.Ready ? pdf.pageCount : 0
            spacing: Kirigami.Units.largeSpacing
            topMargin: Kirigami.Units.largeSpacing
            bottomMargin: Kirigami.Units.largeSpacing
            cacheBuffer: height // render the next page before it scrolls in

            delegate: Rectangle {
                id: page

                required property int index
                readonly property size pointSize: pdf.pagePointSize(index)

                x: (ListView.view.width - width) / 2
                width: pages.pageWidth
                height: pointSize.width > 0 ? width * pointSize.height / pointSize.width : width * 1.414
                color: "white" // paper, also while the page renders

                PdfPageImage {
                    anchors.fill: parent
                    document: pdf
                    currentFrame: page.index
                    asynchronous: true
                    sourceSize.width: page.width * Screen.devicePixelRatio
                    sourceSize.height: page.height * Screen.devicePixelRatio
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
