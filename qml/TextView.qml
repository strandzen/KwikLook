// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami
import org.kde.syntaxhighlighting
import kwiklook

// Text and code, like Kate but read-only: syntax colours (KDE's highlighting, a
// light or dark theme to match the colour scheme), monospace, line numbers, long
// lines wrapped. Text can be selected and copied. Long files show their start.
// Ctrl+wheel zooms (the font size, kept for the next files until the preview closes).
// Markdown shows rendered (a readable column, local pictures only, links open in the
// browser) or as source; a button in the bar switches, and the choice is remembered.
// Ctrl+F finds text (FindBar below); the matches on screen are marked.
ColumnLayout {
    id: view
    objectName: "textView"

    readonly property bool rendered: BarItems.hasFormattedView(Viewer.mimeType) && Viewer.showFormatted

    // The user zoomed (for the zoom indicator).
    signal zoomed()

    readonly property real zoomPercent: Math.round(Viewer.textZoom * 100)

    function zoomBy(factor) {
        const old = Viewer.textZoom;
        let z = Math.max(0.5, Math.min(4, old * factor));
        // Stop at normal size on the way through.
        if ((old < 1 && z > 1) || (old > 1 && z < 1)) {
            z = 1;
        }
        setZoom(z);
    }
    function resetZoom() {
        setZoom(1);
    }
    // Find: where each match starts (positions in the text), and the one shown.
    property var matches: []
    property int currentMatch: -1

    function focusSearch() {
        findBar.focusField();
    }
    function findNext() {
        if (matches.length > 0) {
            showMatch((currentMatch + 1) % matches.length);
        }
    }
    function findPrevious() {
        if (matches.length > 0) {
            showMatch((currentMatch - 1 + matches.length) % matches.length);
        }
    }
    function runSearch() {
        const needle = Viewer.searchOpen ? Viewer.searchText : "";
        const found = [];
        if (needle !== "") {
            const text = area.getText(0, area.length); // plain text, also when rendered
            // Ignore case, unless lower case changes the length (positions would shift).
            const lower = text.toLowerCase();
            const sameLength = lower.length === text.length;
            const hay = sameLength ? lower : text;
            const what = sameLength ? needle.toLowerCase() : needle;
            for (let i = hay.indexOf(what); i >= 0 && found.length < 100000; i = hay.indexOf(what, i + what.length)) {
                found.push(i);
            }
        }
        matches = found;
        currentMatch = -1;
        if (found.length > 0) {
            // The first match from where the view is now.
            const top = area.positionAt(0, scroll.contentItem.contentY);
            const next = found.findIndex(start => start >= top);
            showMatch(next >= 0 ? next : 0);
        }
    }
    function showMatch(index) {
        currentMatch = index;
        const rect = area.positionToRectangle(matches[index]);
        const flick = scroll.contentItem;
        if (rect.y < flick.contentY || rect.y + rect.height > flick.contentY + flick.height) {
            flick.contentY = Math.max(0, Math.min(flick.contentHeight - flick.height, rect.y - flick.height / 3));
        }
    }

    Connections {
        target: Viewer
        function onSearchTextChanged() {
            Qt.callLater(view.runSearch);
        }
        function onSearchOpenChanged() {
            Qt.callLater(view.runSearch);
        }
    }

    function setZoom(z) {
        // Stay at the same place in the text while it reflows.
        const flick = scroll.contentItem;
        const at = flick.contentHeight > 0 ? flick.contentY / flick.contentHeight : 0;
        Viewer.textZoom = z;
        Qt.callLater(() => flick.contentY = at * flick.contentHeight);
        zoomed();
    }

    spacing: 0
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        position: Kirigami.InlineMessage.Position.Header
        visible: text !== ""
        text: Viewer.textNote
    }

    QQC2.ScrollView {
        id: scroll
        Layout.fillWidth: true
        Layout.fillHeight: true
        // Never scroll sideways: long lines wrap.
        QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

        QQC2.TextArea {
            id: area
            objectName: "textArea"

            readonly property real gutterWidth: view.rendered ? 0 : numberMetrics.advanceWidth(String(Math.max(numbers.count, 10))) + Kirigami.Units.largeSpacing * 2
            // Rendered Markdown: a column of readable width, centred.
            readonly property real sideMargin: view.rendered
                ? Math.max(Kirigami.Units.gridUnit, (scroll.width - Kirigami.Units.gridUnit * 40 * Viewer.textZoom) / 2) : 0
            readonly property font baseFont: view.rendered ? Kirigami.Theme.defaultFont : Kirigami.Theme.fixedWidthFont

            readOnly: true
            selectByMouse: true
            textFormat: view.rendered ? TextEdit.MarkdownText : TextEdit.PlainText
            wrapMode: TextEdit.Wrap
            font.family: baseFont.family
            font.pointSize: baseFont.pointSize * Viewer.textZoom
            text: Viewer.text
            onTextChanged: Qt.callLater(view.runSearch) // a new file, or rendered / source
            baseUrl: Viewer.currentUrl // pictures next to the file
            leftPadding: view.rendered ? sideMargin : gutterWidth + Kirigami.Units.largeSpacing
            rightPadding: view.rendered ? sideMargin : Kirigami.Units.largeSpacing
            topPadding: Kirigami.Units.largeSpacing
            bottomPadding: Kirigami.Units.largeSpacing
            // Web and mail links only: a link to a local program must not start it.
            onLinkActivated: link => {
                const scheme = link.split(":")[0].toLowerCase();
                if (scheme === "http" || scheme === "https" || scheme === "mailto") {
                    Qt.openUrlExternally(link);
                }
            }
            background: Rectangle {
                color: Kirigami.Theme.backgroundColor

                // The gutter: a slightly different shade, like Kate's.
                Rectangle {
                    visible: !view.rendered
                    width: area.gutterWidth
                    height: parent.height
                    color: Kirigami.Theme.alternateBackgroundColor
                }
            }

            HoverHandler {
                enabled: view.rendered
                cursorShape: area.hoveredLink !== "" ? Qt.PointingHandCursor : Qt.IBeamCursor
            }

            // Source only: removing it takes its colours off rendered Markdown.
            Loader {
                active: !view.rendered
                sourceComponent: highlighterComponent
            }
            Component {
                id: highlighterComponent

                SyntaxHighlighter {
                    objectName: "highlighter"
                    textEdit: area
                    // By name first (Kate's choice for e.g. CMakeLists.txt), then by the type
                    // read from the content (scripts without an extension). "None": not known.
                    definition: {
                        const byName = Repository.definitionForFileName(Viewer.fileName);
                        return byName.name !== "None" ? byName : Repository.definitionForMimeType(Viewer.mimeType);
                    }
                    theme: Repository.defaultTheme(Kirigami.ColorUtils.brightnessForColor(Kirigami.Theme.backgroundColor) === Kirigami.ColorUtils.Dark
                        ? Repository.DarkTheme : Repository.LightTheme)
                }
            }

            // On the text itself: the scroll view would take the wheel first.
            WheelHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad // Wayland types the mouse as a touchpad
                acceptedModifiers: Qt.ControlModifier
                onWheel: event => view.zoomBy(Math.pow(1.1, event.angleDelta.y / 120))
            }

            FontMetrics {
                id: numberMetrics
                font: area.font
            }

            // The matches on screen, marked under the text (all of them could be thousands).
            Repeater {
                id: matchMarks

                model: {
                    const all = view.matches;
                    if (all.length === 0) {
                        return [];
                    }
                    const flick = scroll.contentItem;
                    area.contentHeight; // laid out again (zoom, width): positions change
                    const first = area.positionAt(0, flick.contentY) - Viewer.searchText.length;
                    const last = area.positionAt(area.width, flick.contentY + scroll.height);
                    let lo = 0;
                    let hi = all.length;
                    while (lo < hi) {
                        const mid = (lo + hi) >> 1;
                        if (all[mid] < first) {
                            lo = mid + 1;
                        } else {
                            hi = mid;
                        }
                    }
                    const shown = [];
                    for (let i = lo; i < all.length && all[i] <= last && shown.length < 500; ++i) {
                        shown.push(i);
                    }
                    return shown;
                }

                Rectangle {
                    required property int modelData // index in view.matches
                    readonly property int start: view.matches[modelData]
                    readonly property rect from: { area.contentHeight; return area.positionToRectangle(start); }
                    readonly property rect to: { area.contentHeight; return area.positionToRectangle(start + Viewer.searchText.length); }

                    objectName: "matchMark"
                    z: -1 // under the text
                    x: from.x
                    y: from.y
                    // A match wrapped onto the next line: marked to the end of its first line.
                    width: to.y === from.y ? to.x - from.x : area.width - area.rightPadding - from.x
                    height: from.height
                    radius: Kirigami.Units.cornerRadius / 2
                    color: Qt.alpha(Kirigami.Theme.highlightColor, modelData === view.currentMatch ? 0.6 : 0.25)
                }
            }

            LineNumbers {
                id: numbers
                document: area.textDocument
                top: scroll.contentItem.contentY - area.topPadding
                height: scroll.height
            }

            // Enough labels for a screenful of one-row lines, kept while scrolling: only
            // their numbers and positions change.
            Repeater {
                id: numberLabels

                readonly property var lines: numbers.lines // one conversion per update, not one per label

                model: Math.ceil(Screen.height / numberMetrics.lineSpacing) + 2

                QQC2.Label {
                    objectName: "lineNumber"
                    required property int index
                    readonly property var line: numberLabels.lines[index]

                    visible: line !== undefined && !view.rendered
                    x: 0
                    y: line ? area.topPadding + line.y : 0
                    width: area.gutterWidth - Kirigami.Units.largeSpacing
                    horizontalAlignment: Text.AlignRight
                    font: area.font
                    color: Kirigami.Theme.disabledTextColor
                    text: line ? line.number : ""
                }
            }
        }
    }

    FindBar {
        id: findBar
        Layout.fillWidth: true
        count: view.matches.length
        current: view.currentMatch
        onNext: view.findNext()
        onPrevious: view.findPrevious()
    }
}
