// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import kwiklook

// The preview's only bar: the file name, centred, and the buttons chosen in the
// settings left and right of it (by default ← →; information, Share, Open with…,
// close). Dragging it moves the window. A standard toolbar (as a Kirigami app's
// header) in the title bar (Header) colours, since it stands in for the title bar.
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

    // One item from the settings: a button, or a stretching gap.
    component BarItem: Loader {
        id: slot
        required property string modelData
        Layout.fillWidth: modelData === "Spacer"
        visible: modelData !== "ViewMode" || BarItems.hasFormattedView(Viewer.mimeType)
        sourceComponent: modelData === "Spacer" ? spacer : button

        Component {
            id: spacer
            Item {}
        }
        Component {
            id: button
            BarButton {
                objectName: "bar" + slot.modelData
                icon.name: BarItems.iconName(slot.modelData)
                text: BarItems.text(slot.modelData)
                checkable: slot.modelData === "Information" || slot.modelData === "ViewMode"
                checked: slot.modelData === "Information" ? Viewer.infoOpen
                    : slot.modelData === "ViewMode" ? Viewer.showFormatted
                    : false
                enabled: slot.modelData === "Previous" ? Viewer.currentIndex > 0
                    : slot.modelData === "Next" ? Viewer.currentIndex + 1 < Viewer.count
                    : true
                onClicked: bar.activate(slot.modelData, this)
            }
        }
    }

    function activate(item, button) {
        switch (item) {
        case "Previous": Viewer.Previous(); break;
        case "Next": Viewer.Next(); break;
        case "Information": Viewer.infoOpen = !Viewer.infoOpen; break;
        case "ViewMode": Viewer.showFormatted = !Viewer.showFormatted; break;
        case "Share": bar.menus.showShare(button); break;
        case "OpenWith": bar.menus.showOpenWith(button); break;
        case "Open": bar.menus.openWithDefault(); break;
        case "ShowInDolphin": bar.menus.showInFileManager(button); break;
        case "CopyPath":
            bar.menus.copyPath();
            bar.window.showPassiveNotification(i18n("Path copied")); // as in the panel
            break;
        case "Close": Viewer.Close(); break;
        }
    }

    // The items left and right of the name, from the settings.
    RowLayout {
        id: row
        anchors.fill: parent
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            id: leftButtons
            Layout.fillWidth: KwikLookSettings.barLeft.includes("Spacer")
            spacing: parent.spacing

            Repeater {
                model: KwikLookSettings.barLeft
                delegate: BarItem {}
            }
        }

        Item {
            Layout.fillWidth: true
        }

        RowLayout {
            id: rightButtons
            Layout.fillWidth: KwikLookSettings.barRight.includes("Spacer")
            spacing: parent.spacing

            Repeater {
                model: KwikLookSettings.barRight
                delegate: BarItem {}
            }
        }
    }

    // Centred in the bar, not between the buttons, like a window title; with several
    // files, the position follows, dimmed (“2 / 5”).
    RowLayout {
        anchors.centerIn: parent
        // Room left by the wider side's buttons (not its stretching gaps).
        width: Math.min(implicitWidth, parent.width - 2 * (Math.max(leftButtons.implicitWidth, rightButtons.implicitWidth) + Kirigami.Units.largeSpacing))
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
