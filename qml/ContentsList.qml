// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// The contents sidebar's list: headings or bookmarks as a tree with lines, headings with
// sub-headings fold in and out; the current one marked (or its folded parent) and kept
// in sight; a click asks the view to go there.
QQC2.ScrollView {
    id: root
    objectName: "contentsTree"

    // [{title, level, …}] in document order.
    property var entries: []
    // The entry the view is at, -1 for none (above the first).
    property int currentIndex: -1

    signal activated(int index)

    // Longer lists start with only the top level open.
    readonly property int foldAbove: 30
    // One tree column: the fold arrow, or the line to a child.
    readonly property real column: Kirigami.Units.iconSizes.small + Kirigami.Units.smallSpacing

    // The tree, per entry: parent (-1 at the top), depth, hasChildren, last (no later
    // sibling), guides (per column left of its own line: an ancestor's line goes on).
    readonly property var tree: {
        const n = entries.length;
        const parent = [], depth = [], hasChildren = new Array(n).fill(false), last = new Array(n).fill(false), guides = [];
        const stack = [];
        for (let i = 0; i < n; ++i) {
            // Levels may skip (1, then 3): the nearest lower level is the parent.
            while (stack.length > 0 && entries[stack[stack.length - 1]].level >= entries[i].level) {
                stack.pop();
            }
            const p = stack.length > 0 ? stack[stack.length - 1] : -1;
            parent.push(p);
            depth.push(stack.length);
            if (p >= 0) {
                hasChildren[p] = true;
            }
            stack.push(i);
        }
        const seen = {};
        for (let i = n - 1; i >= 0; --i) {
            if (!seen[parent[i]]) {
                last[i] = true;
                seen[parent[i]] = true;
            }
        }
        for (let i = 0; i < n; ++i) {
            const g = new Array(Math.max(0, depth[i] - 1)).fill(false);
            for (let a = parent[i]; a >= 0 && depth[a] >= 1; a = parent[a]) {
                g[depth[a] - 1] = !last[a];
            }
            guides.push(g);
        }
        return {parent, depth, hasChildren, last, guides, folds: hasChildren.includes(true)};
    }

    // Folded entries (by index): set again for each new list.
    property var folded: ({})
    onTreeChanged: {
        const f = {};
        if (entries.length > foldAbove) {
            for (let i = 0; i < entries.length; ++i) {
                if (tree.hasChildren[i] && tree.depth[i] >= 1) {
                    f[i] = true;
                }
            }
        }
        folded = f;
    }
    function toggle(index) {
        const f = Object.assign({}, folded);
        if (f[index]) {
            delete f[index];
        } else {
            f[index] = true;
        }
        folded = f;
    }

    // The rows shown (entry indices), and per entry the one shown for it (itself, or
    // the folded parent it's inside).
    readonly property var shown: {
        const rows = [], shownAs = [], rowOf = {};
        for (let i = 0; i < entries.length; ++i) {
            const p = tree.parent[i];
            const visible = p < 0 || (shownAs[p] === p && !folded[p]);
            shownAs.push(visible ? i : shownAs[p]);
            if (visible) {
                rowOf[i] = rows.length;
                rows.push(i);
            }
        }
        return {rows, shownAs, rowOf};
    }
    readonly property int currentEntry: currentIndex >= 0 && currentIndex < shown.shownAs.length ? shown.shownAs[currentIndex] : -1
    readonly property int currentRow: currentEntry >= 0 ? shown.rowOf[currentEntry] : -1

    QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

    onCurrentRowChanged: if (currentRow >= 0) list.positionViewAtIndex(currentRow, ListView.Contain)

    ListView {
        id: list
        objectName: "contentsList"
        model: root.shown.rows
        topMargin: Kirigami.Units.smallSpacing
        bottomMargin: Kirigami.Units.smallSpacing
        reuseItems: true

        delegate: QQC2.ItemDelegate {
            id: row

            required property int modelData // the entry
            readonly property var entry: root.entries[modelData] ?? {title: "", level: 1}
            readonly property int depth: root.tree.depth[modelData] ?? 0
            readonly property bool folds: root.tree.hasChildren[modelData] ?? false
            readonly property bool open: folds && !root.folded[modelData]
            // Tree columns start here; the title after its own column.
            readonly property real treeX: Kirigami.Units.smallSpacing * 2
            function columnCentre(c) {
                return Math.round(treeX + c * root.column + root.column / 2);
            }

            objectName: "contentsEntry"
            width: ListView.view.width
            // The highlight across the whole row; the indentation is inside it.
            leftInset: Kirigami.Units.smallSpacing
            rightInset: Kirigami.Units.smallSpacing
            topInset: 0
            bottomInset: 0
            leftPadding: treeX + (root.tree.folds ? (depth + 1) * root.column : 0)
            rightPadding: Kirigami.Units.largeSpacing
            topPadding: Kirigami.Units.smallSpacing + 1
            bottomPadding: Kirigami.Units.smallSpacing + 1
            highlighted: modelData === root.currentEntry
            text: entry.title !== "" ? entry.title : i18nc("@item a heading without text", "(Untitled)")
            font.bold: depth === 0
            contentItem: QQC2.Label {
                text: row.text
                font: row.font
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
                color: row.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
            }
            QQC2.ToolTip.text: text
            QQC2.ToolTip.visible: hovered && contentItem.truncated
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            onClicked: root.activated(modelData)

            // Ancestors' lines passing by.
            Repeater {
                model: root.tree.guides[row.modelData] ?? []
                Kirigami.Separator {
                    required property int index
                    required property bool modelData
                    visible: modelData
                    x: row.columnCentre(index)
                    height: row.height
                    width: 1
                }
            }
            // The line from the parent: ├ or └.
            Kirigami.Separator {
                visible: row.depth > 0
                x: row.columnCentre(row.depth - 1)
                width: 1
                height: root.tree.last[row.modelData] ? Math.round(row.height / 2) : row.height
            }
            Kirigami.Separator {
                visible: row.depth > 0
                x: row.columnCentre(row.depth - 1)
                y: Math.round(row.height / 2)
                width: row.folds ? root.column - Kirigami.Units.iconSizes.small / 2
                    : row.leftPadding - x - Kirigami.Units.smallSpacing
                height: 1
            }
            // Down to the first child, when open.
            Kirigami.Separator {
                visible: row.open
                x: row.columnCentre(row.depth)
                y: Math.round(row.height / 2 + Kirigami.Units.iconSizes.small / 2)
                width: 1
                height: row.height - y
            }
            // Fold arrow.
            Kirigami.Icon {
                objectName: "contentsFold"
                visible: row.folds
                x: row.columnCentre(row.depth) - width / 2
                anchors.verticalCenter: parent.verticalCenter
                implicitWidth: Kirigami.Units.iconSizes.small
                implicitHeight: Kirigami.Units.iconSizes.small
                source: row.open ? "arrow-down" : row.mirrored ? "arrow-left" : "arrow-right"
                selected: row.highlighted

                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -Kirigami.Units.smallSpacing
                    // The row may be rebuilt by this: change the list afterwards.
                    onClicked: Qt.callLater(root.toggle, row.modelData)
                }
            }
        }
    }
}
