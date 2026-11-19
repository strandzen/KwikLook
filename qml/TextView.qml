// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.syntaxhighlighting
import kwiklook

// Text and code, like Kate but read-only: syntax colours (KDE's highlighting, a
// light or dark theme to match the colour scheme), monospace, line numbers, long
// lines wrapped. Text can be selected and copied. Long files show their start.
// Ctrl+wheel zooms (the font size, kept for the next files until the preview closes).
ColumnLayout {
    id: view
    objectName: "textView"

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

            readonly property real gutterWidth: numberMetrics.advanceWidth(String(Math.max(numbers.count, 10))) + Kirigami.Units.largeSpacing * 2

            readOnly: true
            selectByMouse: true
            textFormat: TextEdit.PlainText
            wrapMode: TextEdit.Wrap
            font.family: Kirigami.Theme.fixedWidthFont.family
            font.pointSize: Kirigami.Theme.fixedWidthFont.pointSize * Viewer.textZoom
            text: Viewer.text
            leftPadding: gutterWidth + Kirigami.Units.largeSpacing
            topPadding: Kirigami.Units.largeSpacing
            bottomPadding: Kirigami.Units.largeSpacing
            background: Rectangle {
                color: Kirigami.Theme.backgroundColor

                // The gutter: a slightly different shade, like Kate's.
                Rectangle {
                    width: area.gutterWidth
                    height: parent.height
                    color: Kirigami.Theme.alternateBackgroundColor
                }
            }

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

                model: Math.ceil(scroll.height / numberMetrics.lineSpacing) + 2

                QQC2.Label {
                    objectName: "lineNumber"
                    required property int index
                    readonly property var line: numberLabels.lines[index]

                    visible: line !== undefined
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
}
