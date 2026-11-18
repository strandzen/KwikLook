// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import kwiklook

// The information panel at the right: the file's details in sections (File,
// Details, Permissions, from Viewer.infoSections), and actions at the bottom.
ColumnLayout {
    id: panel
    objectName: "infoPanel"

    required property Kirigami.ApplicationWindow window

    spacing: 0

    FileMenus {
        id: actions
        url: Viewer.currentUrl
        onLaunched: Viewer.Close()
    }

    QQC2.ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: availableWidth // wrap, never scroll sideways

        ColumnLayout {
            width: parent.width
            spacing: 0

            Repeater {
                model: Viewer.infoSections

                ColumnLayout {
                    id: section

                    required property var modelData

                    Layout.fillWidth: true
                    Layout.bottomMargin: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.ListSectionHeader {
                        Layout.fillWidth: true
                        text: section.modelData.title
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: Kirigami.Units.largeSpacing
                        Layout.rightMargin: Kirigami.Units.largeSpacing
                        columns: 2
                        columnSpacing: Kirigami.Units.largeSpacing
                        rowSpacing: Kirigami.Units.smallSpacing

                        Repeater {
                            model: section.modelData.rows

                            // Labels here, values in the next Repeater: each in its grid column.
                            delegate: QQC2.Label {
                                required property var modelData
                                required property int index
                                Layout.row: index
                                Layout.column: 0
                                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                                color: Kirigami.Theme.disabledTextColor
                                text: modelData.label
                            }
                        }
                        Repeater {
                            model: section.modelData.rows

                            delegate: QQC2.Label {
                                required property var modelData
                                required property int index
                                Layout.row: index
                                Layout.column: 1
                                Layout.fillWidth: true
                                Layout.preferredWidth: 0 // wrapped text: don't widen the panel
                                wrapMode: Text.Wrap
                                text: modelData.value
                            }
                        }
                    }
                }
            }
        }
    }

    Kirigami.Separator {
        Layout.fillWidth: true
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing

        QQC2.Button {
            Layout.fillWidth: true
            icon.name: "system-file-manager"
            text: i18nc("@action:button", "Show in Dolphin")
            onClicked: actions.showInFileManager(this)
        }
        QQC2.Button {
            Layout.fillWidth: true
            icon.name: "edit-copy-path"
            text: i18nc("@action:button", "Copy Path")
            onClicked: {
                actions.copyPath();
                panel.window.showPassiveNotification(i18n("Path copied"));
            }
        }
    }
}
