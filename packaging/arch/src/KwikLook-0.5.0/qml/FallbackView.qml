// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import kwiklook

// For files no other view shows: a large thumbnail (or the file type's icon),
// the name, and one dimmed line with type, size and date. Office documents show here
// while LibreOffice prepares their preview, with a note at the top.
Item {
    id: view
    objectName: "fallbackView"

    // As large as fits, leaving room for the text; never bigger than KDE's largest thumbnails.
    readonly property int previewSize: Math.max(Kirigami.Units.iconSizes.huge,
        Math.min(width - Kirigami.Units.gridUnit * 4, height - Kirigami.Units.gridUnit * 8, 512))

    ColumnLayout {
        anchors.centerIn: parent
        width: parent.width - Kirigami.Units.gridUnit * 4
        spacing: Kirigami.Units.largeSpacing

        FileThumbnail {
            Layout.alignment: Qt.AlignHCenter
            size: view.previewSize
        }

        Kirigami.Heading {
            Layout.fillWidth: true
            Layout.preferredWidth: 0 // wrapped text: don't widen the layout
            level: 2
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            maximumLineCount: 3
            elide: Text.ElideMiddle
            text: Viewer.fileName
        }

        QQC2.Label {
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: Kirigami.Theme.disabledTextColor
            text: Viewer.details
        }

        DelayedBusyIndicator {
            id: busy
            Layout.alignment: Qt.AlignHCenter
            busy: Viewer.previewBusy
        }
    }

    // “Preparing preview…”, or why there is no preview.
    Kirigami.InlineMessage {
        objectName: "previewNote"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        position: Kirigami.InlineMessage.Position.Header
        type: Viewer.previewBusy ? Kirigami.MessageType.Information : Kirigami.MessageType.Warning
        // “Preparing preview…” only when it takes a moment (a cached one is there at once).
        visible: text !== "" && (!Viewer.previewBusy || busy.shown)
        text: Viewer.previewNote
    }
}
