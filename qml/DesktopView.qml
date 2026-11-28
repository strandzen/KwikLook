// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import kwiklook

// .desktop files (application launchers, links): their icon, name and description,
// then what they run or open. The bar's view toggle shows the file's text instead.
QQC2.ScrollView {
    id: view
    objectName: "desktopView"

    readonly property var entry: Viewer.desktopEntry

    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false
    QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

    background: Rectangle {
        color: Kirigami.Theme.backgroundColor
    }

    Item {
        implicitWidth: view.availableWidth
        implicitHeight: column.implicitHeight + Kirigami.Units.gridUnit * 2

        // Centred when it fits, from the top when it scrolls.
        ColumnLayout {
            id: column
            anchors.horizontalCenter: parent.horizontalCenter
            y: Math.max(Kirigami.Units.gridUnit, (view.availableHeight - implicitHeight) / 2)
            width: Math.min(parent.width - Kirigami.Units.gridUnit * 2, Kirigami.Units.gridUnit * 28)
            spacing: Kirigami.Units.largeSpacing

            Kirigami.Icon {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: Kirigami.Units.iconSizes.enormous
                implicitHeight: implicitWidth
                source: view.entry.icon || "application-x-executable"
                fallback: "application-x-executable"
            }
            Kirigami.Heading {
                objectName: "desktopName"
                Layout.fillWidth: true
                Layout.preferredWidth: 0 // wrapped text: don't widen the layout
                level: 1
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: view.entry.name || Viewer.fileName
            }
            QQC2.Label {
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: Kirigami.Theme.disabledTextColor
                visible: text !== ""
                text: [view.entry.genericName, view.entry.comment].filter(part => part).join(" · ")
            }

            // What it runs or opens; empty rows hidden.
            Kirigami.FormLayout {
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.largeSpacing

                QQC2.Label {
                    Kirigami.FormData.label: i18nc("@label what a launcher starts", "Command:")
                    Layout.maximumWidth: column.width / 2
                    wrapMode: Text.WrapAnywhere
                    visible: text !== ""
                    text: view.entry.exec || ""
                }
                QQC2.Label {
                    Kirigami.FormData.label: i18nc("@label what a link opens", "Address:")
                    Layout.maximumWidth: column.width / 2
                    wrapMode: Text.WrapAnywhere
                    visible: text !== ""
                    text: view.entry.url || ""
                }
                QQC2.Label {
                    Kirigami.FormData.label: i18nc("@label menu categories", "Categories:")
                    Layout.maximumWidth: column.width / 2
                    wrapMode: Text.Wrap
                    visible: text !== ""
                    text: view.entry.categories || ""
                }
                QQC2.Label {
                    Kirigami.FormData.label: i18nc("@label", "Runs in:")
                    visible: view.entry.terminal === true
                    text: i18nc("@info runs in", "A terminal")
                }
                QQC2.Label {
                    Kirigami.FormData.label: i18nc("@label", "In menus:")
                    visible: view.entry.hidden === true
                    text: i18nc("@info hidden from menus", "Hidden")
                }
            }
        }
    }
}
