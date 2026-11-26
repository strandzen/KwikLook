// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The note in the view's bottom right corner: where you are among the files (“7 / 30”)
// and the zoom level, in one box when both show (“7 / 30 | 90%”). Each part stays a
// second after its flash; when the last one's second is up, the box fades (the other
// floating labels' look and timing). Its corners nest in the window's: the window's
// radius less the gap between them.
Rectangle {
    id: root

    property alias positionText: positionLabel.text
    property alias zoomText: zoomLabel.text
    // The window's corner radius.
    property real windowRadius: 0
    // From the window's edges (lift it with anchors.bottomMargin over a view's own bar).
    readonly property real gap: Kirigami.Units.smallSpacing

    // A part stays in the box while its second runs, and while the box fades after it.
    property bool positionShown: false
    property bool zoomShown: false

    function flashPosition() {
        positionShown = true;
        opacity = 1;
        positionTimer.restart();
    }
    function flashZoom() {
        zoomShown = true;
        opacity = 1;
        zoomTimer.restart();
    }

    width: row.implicitWidth + Kirigami.Units.largeSpacing * 2
    height: row.implicitHeight + Kirigami.Units.smallSpacing * 2
    radius: Math.max(2, windowRadius - gap)
    color: Kirigami.Theme.backgroundColor
    opacity: 0
    visible: opacity > 0
    onVisibleChanged: if (!visible) {
        positionShown = false;
        zoomShown = false;
    }
    Behavior on opacity {
        NumberAnimation {
            duration: Kirigami.Units.longDuration
        }
    }

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: Kirigami.Units.largeSpacing

        QQC2.Label {
            id: positionLabel
            objectName: "position"
            visible: root.positionShown
        }
        Kirigami.Separator {
            Layout.fillHeight: true
            visible: root.positionShown && root.zoomShown
        }
        QQC2.Label {
            id: zoomLabel
            objectName: "zoomLevel"
            visible: root.zoomShown
        }
    }

    Timer {
        id: positionTimer
        interval: 1000
        onTriggered: {
            if (zoomTimer.running) {
                root.positionShown = false;
            } else {
                root.opacity = 0;
            }
        }
    }
    Timer {
        id: zoomTimer
        interval: 1000
        onTriggered: {
            if (positionTimer.running) {
                root.zoomShown = false;
            } else {
                root.opacity = 0;
            }
        }
    }
}
