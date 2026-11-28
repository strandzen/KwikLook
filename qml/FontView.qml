// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import kwiklook

// Fonts: the font's name in itself, Install Font (or “Installed”), a field for your
// own text, the alphabet, digits and punctuation, then a pangram (or your text) at one
// size: 36 px, changed with − / + or Ctrl+wheel (Ctrl+plus / minus / 0). Text and size
// stay for the next fonts until the preview closes.
QQC2.ScrollView {
    id: view
    objectName: "fontView"

    // The sample in the font: family and style of the loaded file (a Bold file shows bold).
    component Sample: QQC2.Label {
        Layout.fillWidth: true
        Layout.preferredWidth: 0 // wrapped text: don't widen the layout
        wrapMode: Text.Wrap
        font.family: Viewer.fontFamily
        font.styleName: Viewer.fontStyle
    }

    // The user zoomed (for the zoom indicator, as in the other views).
    signal zoomed()

    readonly property real zoomPercent: Math.round(Viewer.fontSampleSize / 36 * 100)

    function zoomBy(factor) {
        setSize(Viewer.fontSampleSize * factor);
    }
    function resetZoom() {
        setSize(36);
    }
    function setSize(size) {
        Viewer.fontSampleSize = Math.round(Math.max(8, Math.min(288, size)));
        zoomed();
    }
    // − / +: through the usual type sizes.
    readonly property var steps: [8, 10, 12, 14, 18, 24, 28, 36, 48, 60, 72, 96, 120, 144, 192, 240, 288]
    function stepSize(up) {
        const size = Viewer.fontSampleSize;
        const next = up ? steps.find(step => step > size) : steps.slice().reverse().find(step => step < size);
        if (next !== undefined) {
            setSize(next);
        }
    }

    readonly property string sampleText: Viewer.fontSampleText !== "" ? Viewer.fontSampleText
        : i18nc("@info sample text showing every letter of the alphabet; translate to a pangram of your language", "The quick brown fox jumps over the lazy dog")

    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false
    // Never scroll sideways: long text wraps.
    QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

    background: Rectangle {
        color: Kirigami.Theme.backgroundColor
    }

    WheelHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad // Wayland types the mouse as a touchpad
        acceptedModifiers: Qt.ControlModifier
        onWheel: event => view.zoomBy(Math.pow(1.1, event.angleDelta.y / 120))
    }

    Item {
        // As wide as the view; the column inside is centred and capped in width.
        implicitWidth: view.availableWidth
        implicitHeight: column.implicitHeight + Kirigami.Units.gridUnit * 2

        ColumnLayout {
            id: column
            anchors.horizontalCenter: parent.horizontalCenter
            y: Kirigami.Units.gridUnit
            width: Math.min(parent.width - Kirigami.Units.gridUnit * 2, Kirigami.Units.gridUnit * 40)
            spacing: Kirigami.Units.largeSpacing

            Sample {
                objectName: "fontName"
                font.pointSize: Kirigami.Theme.defaultFont.pointSize * 2.5
                text: Viewer.fontStyle !== "" ? Viewer.fontFamily + " " + Viewer.fontStyle : Viewer.fontFamily
            }
            QQC2.Label {
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                wrapMode: Text.Wrap
                color: Kirigami.Theme.disabledTextColor
                text: Viewer.details
            }

            QQC2.Button {
                id: installButton
                objectName: "installFont"
                visible: !Viewer.fontInstalled
                icon.name: "font-enable"
                text: i18nc("@action:button", "Install Font")
                onClicked: {
                    enabled = false;
                    Viewer.installFont();
                }
                Connections {
                    target: Viewer
                    function onFontInstallFinished(ok) {
                        installButton.enabled = true;
                        const window = view.QQC2.ApplicationWindow.window;
                        if (window) {
                            window.showPassiveNotification(ok ? i18nc("@info", "Font installed. Applications see it after they restart.")
                                                              : i18nc("@info", "Could not install the font."));
                        }
                    }
                }
            }
            RowLayout {
                objectName: "fontInstalled"
                visible: Viewer.fontInstalled
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    implicitWidth: Kirigami.Units.iconSizes.small
                    implicitHeight: implicitWidth
                    source: "checkmark"
                    color: Kirigami.Theme.disabledTextColor
                    isMask: true
                }
                QQC2.Label {
                    color: Kirigami.Theme.disabledTextColor
                    text: i18nc("@info the font is already installed", "Installed")
                }
            }

            QQC2.TextField {
                objectName: "fontSampleField"
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.largeSpacing
                placeholderText: i18nc("@info:placeholder", "Type your own text…")
                text: Viewer.fontSampleText
                onTextEdited: Viewer.fontSampleText = text
            }

            // The character set.
            Sample {
                Layout.topMargin: Kirigami.Units.largeSpacing
                font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.6
                text: "ABCDEFGHIJKLMNOPQRSTUVWXYZ\nabcdefghijklmnopqrstuvwxyz\n0123456789 .,:;!?&@#%*()[]{}\"'-+=/"
            }

            Kirigami.Separator {
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.largeSpacing
                Layout.bottomMargin: Kirigami.Units.largeSpacing
            }

            // The sample at one size, with − / +.
            RowLayout {
                spacing: Kirigami.Units.smallSpacing

                QQC2.ToolButton {
                    objectName: "fontSmaller"
                    display: QQC2.AbstractButton.IconOnly
                    icon.name: "zoom-out"
                    text: i18nc("@action:button font size", "Smaller")
                    enabled: Viewer.fontSampleSize > 8
                    onClicked: view.stepSize(false)
                    QQC2.ToolTip.text: text
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                }
                QQC2.Label {
                    objectName: "fontSize"
                    Layout.minimumWidth: sizeMetrics.advanceWidth("288 px")
                    horizontalAlignment: Text.AlignHCenter
                    color: Kirigami.Theme.disabledTextColor
                    font.features: { "tnum": 1 }
                    text: i18nc("@info font size in pixels", "%1 px", Viewer.fontSampleSize)

                    FontMetrics {
                        id: sizeMetrics
                    }
                }
                QQC2.ToolButton {
                    objectName: "fontLarger"
                    display: QQC2.AbstractButton.IconOnly
                    icon.name: "zoom-in"
                    text: i18nc("@action:button font size", "Larger")
                    enabled: Viewer.fontSampleSize < 288
                    onClicked: view.stepSize(true)
                    QQC2.ToolTip.text: text
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                }
            }
            Sample {
                objectName: "fontSample"
                font.pixelSize: Viewer.fontSampleSize
                text: view.sampleText
            }
        }
    }
}
