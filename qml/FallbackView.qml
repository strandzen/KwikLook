// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami
import kwiklook

// For files no other view shows: a large thumbnail (or the file type's icon),
// the name, and one dimmed line with type, size and date.
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

        Item {
            Layout.alignment: Qt.AlignHCenter
            implicitWidth: view.previewSize
            implicitHeight: view.previewSize

            Image {
                id: thumbnail
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: false
                sourceSize.width: view.previewSize * Screen.devicePixelRatio
                sourceSize.height: view.previewSize * Screen.devicePixelRatio
                source: Viewer.fileExists
                    ? "image://thumbnail/" + encodeURIComponent(Viewer.mimeType) + "/" + encodeURIComponent(Viewer.currentUrl.toString())
                    : ""
                visible: status === Image.Ready && implicitWidth > 1 // 1×1: no thumbnail
            }

            // Until the thumbnail is there, and when there is none.
            Kirigami.Icon {
                anchors.centerIn: parent
                width: Math.min(parent.width, Kirigami.Units.iconSizes.enormous * 2)
                height: width
                source: Viewer.iconName
                fallback: "unknown"
                visible: !thumbnail.visible && Viewer.iconName !== ""
            }
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
    }
}
