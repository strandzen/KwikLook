// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import kwiklook

// The find bar below text and PDFs, like Kate's: the search text, “3 of 12”, previous,
// next, close. Enter finds the next match, Shift+Enter the previous one; Esc closes
// (Main.qml). Case doesn't matter. What it looks for is Viewer.searchText; the view
// searches and tells the bar how many matches there are.
QQC2.ToolBar {
    id: bar
    objectName: "findBar"

    // Matches found, and which one is shown (0-based; -1: none).
    property int count: 0
    property int current: -1
    // Still searching (PDFs search page by page).
    property bool busy: false

    signal next()
    signal previous()

    function focusField() {
        field.forceActiveFocus();
        field.selectAll();
    }

    position: QQC2.ToolBar.Footer
    visible: Viewer.searchOpen
    onVisibleChanged: if (visible) focusField()

    RowLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.smallSpacing

        QQC2.TextField {
            id: field
            objectName: "findField"
            Layout.fillWidth: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 20
            placeholderText: i18nc("@info:placeholder", "Find…")
            text: Viewer.searchText
            onTextEdited: Viewer.searchText = text
            Keys.onReturnPressed: event => event.modifiers & Qt.ShiftModifier ? bar.previous() : bar.next()
            Keys.onEnterPressed: event => event.modifiers & Qt.ShiftModifier ? bar.previous() : bar.next()
        }
        QQC2.Label {
            objectName: "findCount"
            color: Kirigami.Theme.disabledTextColor
            text: Viewer.searchText === "" ? ""
                : bar.count > 0 ? i18nc("@info search result, e.g. 3 of 12", "%1 of %2", bar.current + 1, bar.count)
                : bar.busy ? i18nc("@info still searching", "Searching…")
                : i18nc("@info search result", "Not found")
        }
        Item {
            Layout.fillWidth: true
        }
        QQC2.ToolButton {
            display: QQC2.AbstractButton.IconOnly
            icon.name: "go-up"
            text: i18nc("@action:button", "Previous Match")
            enabled: bar.count > 0
            onClicked: bar.previous()
            QQC2.ToolTip.text: text
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }
        QQC2.ToolButton {
            display: QQC2.AbstractButton.IconOnly
            icon.name: "go-down"
            text: i18nc("@action:button", "Next Match")
            enabled: bar.count > 0
            onClicked: bar.next()
            QQC2.ToolTip.text: text
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }
        QQC2.ToolButton {
            display: QQC2.AbstractButton.IconOnly
            icon.name: "dialog-close"
            text: i18nc("@action:button close the find bar", "Close")
            onClicked: Viewer.searchOpen = false
            QQC2.ToolTip.text: text
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }
    }
}
