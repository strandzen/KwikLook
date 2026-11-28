// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import kwiklook

// CSV and TSV files as a table: the first row as column headers, rows numbered,
// columns as wide as their text (within limits). Long files show their start. The
// bar's view toggle shows the file's text instead (where Ctrl+F finds).
ColumnLayout {
    id: view
    objectName: "csvView"

    spacing: 0
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        position: Kirigami.InlineMessage.Position.Header
        visible: text !== ""
        text: Viewer.textNote
    }

    FontMetrics {
        id: metrics
    }

    GridLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        columns: 2
        rowSpacing: 0
        columnSpacing: 0

        Item {
            implicitWidth: rowNumbers.width
            implicitHeight: headers.height
        }
        QQC2.HorizontalHeaderView {
            id: headers
            objectName: "csvHeaders"
            Layout.fillWidth: true
            syncView: table
            clip: true
        }
        QQC2.VerticalHeaderView {
            id: rowNumbers
            Layout.fillHeight: true
            syncView: table
            clip: true
        }
        QQC2.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            background: Rectangle {
                color: Kirigami.Theme.backgroundColor
            }

            TableView {
                id: table
                objectName: "csvTable"
                model: Viewer.table
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                // From the longest text among the first rows: no remeasuring while scrolling.
                columnWidthProvider: column => Math.max(Kirigami.Units.gridUnit * 3,
                    Math.min(Kirigami.Units.gridUnit * 20, metrics.averageCharacterWidth * Viewer.table.columnLength(column) + Kirigami.Units.largeSpacing * 2))

                delegate: Rectangle {
                    required property string display
                    required property int row

                    implicitHeight: cell.implicitHeight + Kirigami.Units.smallSpacing * 2
                    color: row % 2 ? Kirigami.Theme.alternateBackgroundColor : Kirigami.Theme.backgroundColor

                    QQC2.Label {
                        id: cell
                        anchors.fill: parent
                        anchors.leftMargin: Kirigami.Units.largeSpacing
                        anchors.rightMargin: Kirigami.Units.largeSpacing
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        text: parent.display
                    }
                }
            }
        }
    }
}
