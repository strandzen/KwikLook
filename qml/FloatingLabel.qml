// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// A short note floating over a view (zoom level, page number): flash() shows it, it
// fades out a second after the last flash.
Rectangle {
    id: root

    property alias text: label.text

    function flash() {
        opacity = 1;
        hideTimer.restart();
    }

    width: label.implicitWidth + Kirigami.Units.largeSpacing * 2
    height: label.implicitHeight + Kirigami.Units.smallSpacing * 2
    radius: Kirigami.Units.cornerRadius
    color: Kirigami.Theme.backgroundColor
    opacity: 0
    visible: opacity > 0
    Behavior on opacity {
        NumberAnimation {
            duration: Kirigami.Units.longDuration
        }
    }

    QQC2.Label {
        id: label
        anchors.centerIn: parent
    }
    Timer {
        id: hideTimer
        interval: 1000
        onTriggered: root.opacity = 0
    }
}
