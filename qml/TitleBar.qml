// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import kwiklook

// The preview's only bar: ← →, the file name, Share, Open with…, close. Dragging it
// moves the window. A standard toolbar (as a Kirigami app's header) in the title bar
// (Header) colours, since it stands in for the title bar.
QQC2.ToolBar {
    id: bar

    required property Kirigami.ApplicationWindow window
    required property FileMenus menus

    component BarButton: QQC2.ToolButton {
        display: QQC2.AbstractButton.IconOnly
        QQC2.ToolTip.text: text
        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
    }

    position: QQC2.ToolBar.Header
    // Several children: the bar can't size itself from them.
    contentHeight: row.implicitHeight
    Kirigami.Theme.colorSet: Kirigami.Theme.Header
    Kirigami.Theme.inherit: false

    background: Rectangle {
        color: Kirigami.Theme.backgroundColor
        Kirigami.Separator {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
        }
    }

    DragHandler {
        target: null
        grabPermissions: PointerHandler.CanTakeOverFromAnything
        onActiveChanged: if (active) bar.window.startSystemMove()
    }

    RowLayout {
        id: row
        anchors.fill: parent
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            id: leftButtons
            spacing: parent.spacing

            BarButton {
                icon.name: "go-previous"
                text: i18nc("@action:button", "Previous File")
                enabled: Viewer.currentIndex > 0
                onClicked: Viewer.Previous()
            }
            BarButton {
                icon.name: "go-next"
                text: i18nc("@action:button", "Next File")
                enabled: Viewer.currentIndex + 1 < Viewer.count
                onClicked: Viewer.Next()
            }
        }

        Item {
            Layout.fillWidth: true
        }

        RowLayout {
            id: rightButtons
            spacing: parent.spacing

            BarButton {
                icon.name: "documentinfo"
                text: i18nc("@action:button", "Information")
                checkable: true
                checked: Viewer.infoOpen
                onToggled: Viewer.infoOpen = checked
            }
            BarButton {
                icon.name: "document-share"
                text: i18nc("@action:button", "Share")
                onClicked: bar.menus.showShare(this)
            }
            BarButton {
                icon.name: "document-open"
                text: i18nc("@action:button", "Open With…")
                onClicked: bar.menus.showOpenWith(this)
            }
            BarButton {
                icon.name: "window-close"
                text: i18nc("@action:button", "Close")
                onClicked: Viewer.Close()
            }
        }
    }

    // Centred in the bar, not between the buttons, like a window title; with several
    // files, the position follows, dimmed (“2 / 5”).
    RowLayout {
        anchors.centerIn: parent
        width: Math.min(implicitWidth, parent.width - 2 * (Math.max(leftButtons.width, rightButtons.width) + Kirigami.Units.largeSpacing))
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Heading {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            level: 5
            elide: Text.ElideMiddle
            horizontalAlignment: Text.AlignHCenter
            text: Viewer.fileName
        }
        QQC2.Label {
            objectName: "position"
            visible: Viewer.count > 1
            color: Kirigami.Theme.disabledTextColor
            text: i18nc("@info position in the list of files, e.g. 2 / 5", "%1 / %2", Viewer.currentIndex + 1, Viewer.count)
        }
    }
}
