// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import kwiklook

// The settings window's editor for the preview's bar, like Plasma's for window
// decoration buttons: a copy of the bar with the file name fixed in the middle, and
// the items not in it below. Drag items into the bar, along it, or out of it (back
// below). The flexible space can be used several times. Holds the values until the
// settings window applies them.
Item {
    id: editor

    // Items left and right of the file name (names from BarItems).
    property var leftItems: []
    property var rightItems: []
    // Not in the bar, and always the flexible space.
    readonly property var available: BarItems.buttons.filter(item => !leftItems.includes(item) && !rightItems.includes(item)).concat(["Spacer"])

    // While dragging: what, from where, and where it would land.
    property string dragItem
    property string dragZone
    property int dragIndex: -1
    property string dropZone
    property int dropIndex: -1

    // Moves the item at `fromIndex` of `from` to `toIndex` of `to` (zones: "left",
    // "right", "available"). Into "available": out of the bar. From "available": a
    // copy for the flexible space, which stays available.
    function move(from, fromIndex, to, toIndex) {
        const item = from === "left" ? leftItems[fromIndex] : from === "right" ? rightItems[fromIndex] : available[fromIndex];
        if (item === undefined || (from === "available" && to === "available")) {
            return;
        }
        const l = Array.from(leftItems);
        const r = Array.from(rightItems);
        if (from === "left") {
            l.splice(fromIndex, 1);
        } else if (from === "right") {
            r.splice(fromIndex, 1);
        }
        if (from === to && fromIndex < toIndex) {
            --toIndex; // the item came out before the place
        }
        if (to === "left") {
            l.splice(Math.min(toIndex, l.length), 0, item);
        } else if (to === "right") {
            r.splice(Math.min(toIndex, r.length), 0, item);
        }
        leftItems = l;
        rightItems = r;
    }

    implicitHeight: content.implicitHeight

    // An item as it shows in the editor: in the bar like the bar's button (icon only,
    // the flexible space wider), below with its name.
    component Chip: QQC2.ToolButton {
        id: chip

        required property string modelData
        required property int index
        required property string zone

        objectName: "chip" + zone + modelData
        icon.name: BarItems.iconName(modelData)
        text: BarItems.text(modelData)
        display: zone === "available" ? QQC2.AbstractButton.TextBesideIcon : QQC2.AbstractButton.IconOnly
        width: zone !== "available" && modelData === "Spacer" ? Kirigami.Units.gridUnit * 3 : implicitWidth
        opacity: editor.dragZone === zone && editor.dragIndex === index ? 0.3 : 1
        focusPolicy: Qt.NoFocus
        QQC2.ToolTip.text: text
        QQC2.ToolTip.visible: hovered && zone !== "available" && !drag.active
        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

        // A dashed frame marks the flexible space in the bar.
        Rectangle {
            anchors.fill: parent
            visible: chip.zone !== "available" && chip.modelData === "Spacer"
            color: "transparent"
            radius: Kirigami.Units.cornerRadius
            border.color: Kirigami.Theme.disabledTextColor
            border.width: 1
        }

        DragHandler {
            id: drag
            target: null
            // The page scrolls on a vertical drag too: the chip's drag wins, and keeps it.
            grabPermissions: PointerHandler.CanTakeOverFromAnything | PointerHandler.TakeOverForbidden
            cursorShape: active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            onActiveChanged: {
                if (active) {
                    editor.dragItem = chip.modelData;
                    editor.dragZone = chip.zone;
                    editor.dragIndex = chip.index;
                } else {
                    // The move rebuilds the rows, deleting this chip: after this handler.
                    const ed = editor;
                    const from = ed.dragZone;
                    const fromIndex = ed.dragIndex;
                    const to = ed.dropZone;
                    const toIndex = ed.dropIndex;
                    ed.dragItem = "";
                    ed.dragZone = "";
                    ed.dragIndex = -1;
                    ed.dropZone = "";
                    ed.dropIndex = -1;
                    if (to !== "") {
                        Qt.callLater(() => ed.move(from, fromIndex, to, toIndex));
                    }
                }
            }
            onCentroidChanged: {
                if (active) {
                    const p = editor.mapFromItem(null, centroid.scenePosition);
                    ghost.x = p.x - ghost.width / 2;
                    ghost.y = p.y - ghost.height / 2;
                }
            }
        }
    }

    // Where dragged items can land; tells the editor the place under the pointer. In the
    // bar one row (the right side aligned to the right, as in the bar), below wrapping.
    component Zone: DropArea {
        id: area

        required property string zone
        property var items: []
        readonly property bool wrap: zone === "available"
        readonly property Repeater repeater: wrap ? flowRepeater : rowRepeater
        readonly property Item positioner: wrap ? flow : row

        // Places between the items: before item i, or after the last.
        function indexAt(x) {
            for (let i = 0; i < repeater.count; ++i) {
                const item = repeater.itemAt(i);
                if (x < item.x + item.width / 2) {
                    return i;
                }
            }
            return repeater.count;
        }
        function track(drag) {
            editor.dropZone = zone;
            editor.dropIndex = indexAt(drag.x - positioner.x);
        }

        implicitHeight: positioner.implicitHeight
        keys: ["kwiklook-bar-item"]
        onEntered: drag => track(drag)
        onPositionChanged: drag => track(drag)
        onExited: if (editor.dropZone === zone) editor.dropZone = ""

        Row {
            id: row
            visible: !area.wrap
            x: area.zone === "right" ? area.width - width : 0
            anchors.verticalCenter: parent.verticalCenter
            spacing: Kirigami.Units.smallSpacing

            Repeater {
                id: rowRepeater
                model: area.wrap ? [] : area.items
                delegate: Chip {
                    zone: area.zone
                }
            }
        }
        Flow {
            id: flow
            visible: area.wrap
            width: area.width
            anchors.verticalCenter: parent.verticalCenter
            spacing: Kirigami.Units.smallSpacing

            Repeater {
                id: flowRepeater
                model: area.wrap ? area.items : []
                delegate: Chip {
                    zone: area.zone
                }
            }
        }

        // The landing place, in the bar.
        Rectangle {
            visible: !area.wrap && editor.dropZone === area.zone && editor.dragItem !== ""
            width: 2
            height: Math.max(row.height, Kirigami.Units.iconSizes.medium)
            anchors.verticalCenter: parent.verticalCenter
            x: {
                const n = area.repeater.count;
                if (n === 0 || editor.dropIndex < 0) {
                    return area.zone === "right" ? area.width - width : 0;
                }
                const item = area.repeater.itemAt(Math.min(editor.dropIndex, n - 1));
                return row.x + (item ? (editor.dropIndex < n ? item.x : item.x + item.width) - 1 : 0);
            }
            color: Kirigami.Theme.highlightColor
        }
    }

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: Kirigami.Units.largeSpacing

        QQC2.Label {
            Layout.fillWidth: true
            Layout.preferredWidth: 0 // wrapped text: don't widen the layout
            wrapMode: Text.Wrap
            color: Kirigami.Theme.disabledTextColor
            text: i18n("Drag buttons into the bar, along it or out of it. The keys keep working without their buttons.")
        }

        // The bar, as the preview shows it.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: barRow.implicitHeight + Kirigami.Units.smallSpacing * 2
            radius: Kirigami.Units.cornerRadius
            Kirigami.Theme.colorSet: Kirigami.Theme.Header
            Kirigami.Theme.inherit: false
            color: Kirigami.Theme.backgroundColor
            border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.2)

            RowLayout {
                id: barRow
                anchors.fill: parent
                anchors.margins: Kirigami.Units.smallSpacing
                spacing: Kirigami.Units.largeSpacing

                Zone {
                    objectName: "zoneLeft"
                    Layout.fillWidth: true
                    Layout.minimumHeight: Kirigami.Units.iconSizes.medium + Kirigami.Units.smallSpacing * 2
                    zone: "left"
                    items: editor.leftItems
                }
                QQC2.Label {
                    color: Kirigami.Theme.disabledTextColor
                    text: i18nc("@info stands for the file name in the bar", "File name")
                }
                Zone {
                    objectName: "zoneRight"
                    Layout.fillWidth: true
                    Layout.minimumHeight: Kirigami.Units.iconSizes.medium + Kirigami.Units.smallSpacing * 2
                    zone: "right"
                    items: editor.rightItems
                }
            }
        }

        QQC2.Label {
            Layout.topMargin: Kirigami.Units.smallSpacing
            text: i18nc("@label items that can be put in the bar", "Available:")
        }
        Zone {
            objectName: "zoneAvailable"
            Layout.fillWidth: true
            Layout.minimumHeight: Kirigami.Units.gridUnit * 2
            zone: "available"
            items: editor.available
        }
    }

    // Follows the pointer while dragging (outside the layout); the drop areas see it.
    Rectangle {
        id: ghost
        objectName: "dragGhost"
        visible: editor.dragItem !== ""
        z: 10
        width: Kirigami.Units.iconSizes.medium + Kirigami.Units.smallSpacing * 2
        height: width
        radius: Kirigami.Units.cornerRadius
        color: Qt.alpha(Kirigami.Theme.highlightColor, 0.3)
        border.color: Kirigami.Theme.highlightColor
        Drag.active: visible
        Drag.keys: ["kwiklook-bar-item"]
        Drag.hotSpot.x: width / 2
        Drag.hotSpot.y: height / 2

        Kirigami.Icon {
            anchors.centerIn: parent
            width: Kirigami.Units.iconSizes.smallMedium
            height: width
            source: BarItems.iconName(editor.dragItem)
        }
    }
}
