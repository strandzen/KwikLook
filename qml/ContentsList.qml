// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// The contents sidebar's list: headings or bookmarks, indented by level, the current
// one marked and kept in sight; a click asks the view to go there.
QQC2.ScrollView {
    id: root

    // [{title, level, …}] in document order.
    property var entries: []
    // The entry the view is at, -1 for none (above the first).
    property int currentIndex: -1

    signal activated(int index)

    QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

    onCurrentIndexChanged: if (currentIndex >= 0) list.positionViewAtIndex(currentIndex, ListView.Contain)

    ListView {
        id: list
        objectName: "contentsList"
        model: root.entries
        topMargin: Kirigami.Units.smallSpacing
        bottomMargin: Kirigami.Units.smallSpacing
        reuseItems: true

        delegate: QQC2.ItemDelegate {
            required property int index
            required property var modelData

            objectName: "contentsEntry"
            width: ListView.view.width
            leftPadding: Kirigami.Units.largeSpacing + Kirigami.Units.gridUnit * Math.min(modelData.level - 1, 5)
            rightPadding: Kirigami.Units.largeSpacing
            highlighted: index === root.currentIndex
            text: modelData.title !== "" ? modelData.title : i18nc("@item a heading without text", "(Untitled)")
            font.bold: modelData.level === 1
            QQC2.ToolTip.text: text
            QQC2.ToolTip.visible: hovered && contentItem.truncated === true
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            onClicked: root.activated(index)
        }
    }
}
