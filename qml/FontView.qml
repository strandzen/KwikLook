// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import kwiklook

// Fonts: the font's name in itself, Install Font (or “Installed”), a field for your
// own text, the alphabet, digits and punctuation, then a pangram (or your text) at
// several sizes. Your text stays for the next fonts until the preview closes.
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

    readonly property string sampleText: Viewer.fontSampleText !== "" ? Viewer.fontSampleText
        : i18nc("@info sample text showing every letter of the alphabet; translate to a pangram of your language", "The quick brown fox jumps over the lazy dog")

    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false
    // Never scroll sideways: long text wraps.
    QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

    background: Rectangle {
        color: Kirigami.Theme.backgroundColor
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

            // The sample text at several sizes, each with its size.
            Repeater {
                model: [12, 18, 24, 36, 48, 72]

                ColumnLayout {
                    required property int modelData
                    Layout.fillWidth: true
                    spacing: 0

                    QQC2.Label {
                        color: Kirigami.Theme.disabledTextColor
                        font: Kirigami.Theme.smallFont
                        text: i18nc("@info font size in points", "%1 pt", parent.modelData)
                    }
                    Sample {
                        objectName: "fontSample"
                        font.pointSize: parent.modelData
                        text: view.sampleText
                    }
                }
            }
        }
    }
}
