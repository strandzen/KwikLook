// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// A side panel's heading row (“Contents”), with a line under it.
Item {
    id: root

    property alias text: label.text

    implicitHeight: label.implicitHeight + Kirigami.Units.largeSpacing * 2

    Kirigami.Heading {
        id: label
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Kirigami.Units.largeSpacing
        anchors.rightMargin: Kirigami.Units.largeSpacing
        anchors.verticalCenter: parent.verticalCenter
        level: 4
        elide: Text.ElideRight
    }
    Kirigami.Separator {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }
}
