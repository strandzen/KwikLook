// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import kwiklook

// KwikLook Settings: a normal window from the “KwikLook Settings” launcher entry.
// Changes wait for OK or Apply; Cancel (or closing the window) drops them.
Kirigami.ApplicationWindow {
    id: root

    // The values shown, applied to KwikLookSettings on OK / Apply.
    property bool matchBreeze
    property int radius
    property string openKey
    property bool closeOnFocusLoss
    property bool startAtLogin
    property bool officePreviews
    property bool autoplayMedia
    property bool startMuted
    // LibreOffice installed (checked each time the window opens).
    property bool officeAvailable

    readonly property bool changed: matchBreeze !== KwikLookSettings.matchBreezeCorners
        || radius !== KwikLookSettings.cornerRadius
        || openKey !== KwikLookSettings.openKey
        || closeOnFocusLoss !== KwikLookSettings.closeOnFocusLoss
        || startAtLogin !== autostart.enabled
        || officePreviews !== KwikLookSettings.officePreviews
        || autoplayMedia !== KwikLookSettings.autoplayMedia
        || startMuted !== KwikLookSettings.startMuted
        || JSON.stringify(barEditor.leftItems) !== JSON.stringify(KwikLookSettings.barLeft)
        || JSON.stringify(barEditor.rightItems) !== JSON.stringify(KwikLookSettings.barRight)

    function reset() {
        matchBreeze = KwikLookSettings.matchBreezeCorners;
        radius = KwikLookSettings.cornerRadius;
        openKey = KwikLookSettings.openKey;
        closeOnFocusLoss = KwikLookSettings.closeOnFocusLoss;
        startAtLogin = autostart.enabled;
        officePreviews = KwikLookSettings.officePreviews;
        autoplayMedia = KwikLookSettings.autoplayMedia;
        startMuted = KwikLookSettings.startMuted;
        barEditor.leftItems = KwikLookSettings.barLeft;
        barEditor.rightItems = KwikLookSettings.barRight;
        officeAvailable = Viewer.officePreviewsAvailable();
        errorMessage.visible = false;
    }

    function apply() {
        KwikLookSettings.matchBreezeCorners = matchBreeze;
        KwikLookSettings.cornerRadius = radius;
        KwikLookSettings.openKey = openKey;
        KwikLookSettings.closeOnFocusLoss = closeOnFocusLoss;
        KwikLookSettings.officePreviews = officePreviews;
        KwikLookSettings.autoplayMedia = autoplayMedia;
        KwikLookSettings.startMuted = startMuted;
        KwikLookSettings.barLeft = barEditor.leftItems;
        KwikLookSettings.barRight = barEditor.rightItems;
        KwikLookSettings.save();
        if (!autostart.setEnabled(startAtLogin)) {
            startAtLogin = autostart.enabled;
            errorMessage.visible = true;
            return false;
        }
        return true;
    }

    title: i18nc("@title:window", "Settings") // Kirigami adds “— KwikLook”
    width: Kirigami.Units.gridUnit * 28
    height: Kirigami.Units.gridUnit * 44 // fits most screens; the page scrolls for the rest
    minimumWidth: Kirigami.Units.gridUnit * 20
    minimumHeight: Kirigami.Units.gridUnit * 16
    visible: false

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    onVisibleChanged: if (visible) reset()

    Autostart {
        id: autostart
    }

    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.None
    pageStack.initialPage: FormCard.FormCardPage {
        Kirigami.InlineMessage {
            id: errorMessage
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.largeSpacing
            type: Kirigami.MessageType.Error
            text: i18n("Could not change whether KwikLook starts at login.")
        }

        FormCard.FormHeader {
            title: i18nc("@title:group", "Appearance")
        }
        FormCard.FormCard {
            FormCard.FormRadioDelegate {
                text: i18nc("@option:radio corner radius", "Rounded corners like Breeze windows")
                description: i18n("Follows the window decoration settings.")
                checked: root.matchBreeze
                onToggled: root.matchBreeze = true
            }
            FormCard.FormRadioDelegate {
                id: customRadius
                text: i18nc("@option:radio corner radius", "Custom corner radius")
                checked: !root.matchBreeze
                onToggled: root.matchBreeze = false
            }
            FormCard.FormSpinBoxDelegate {
                label: i18nc("@label:spinbox", "Radius in pixels")
                enabled: !root.matchBreeze
                from: 0
                to: 20
                value: root.radius
                onValueModified: root.radius = value
            }
        }

        FormCard.FormHeader {
            title: i18nc("@title:group", "Bar")
        }
        FormCard.FormCard {
            BarEditor {
                id: barEditor
                objectName: "barEditor"
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.largeSpacing
            }
            FormCard.FormDelegateSeparator {}
            FormCard.FormButtonDelegate {
                objectName: "barReset"
                icon.name: "edit-reset"
                text: i18nc("@action:button", "Reset Bar to Defaults")
                onClicked: {
                    barEditor.leftItems = KwikLookSettings.defaultBarLeftValue;
                    barEditor.rightItems = KwikLookSettings.defaultBarRightValue;
                }
            }
        }

        FormCard.FormHeader {
            title: i18nc("@title:group", "Behaviour")
        }
        FormCard.FormCard {
            FormCard.FormComboBoxDelegate {
                text: i18nc("@label:listbox", "Open the preview in Dolphin with")
                description: i18n("The same key closes it again.")
                textRole: "text"
                valueRole: "value"
                model: [
                    { text: i18nc("@item:inlistbox key", "Space"), value: "Space" },
                    { text: i18nc("@item:inlistbox key", "Shift+Space"), value: "Shift+Space" },
                    { text: i18nc("@item:inlistbox key", "Ctrl+Space"), value: "Ctrl+Space" }
                ]
                currentIndex: Math.max(0, indexOfValue(root.openKey))
                onActivated: root.openKey = currentValue
            }
            FormCard.FormDelegateSeparator {}
            FormCard.FormSwitchDelegate {
                text: i18nc("@option:check", "Close the preview when you switch to another window")
                checked: root.closeOnFocusLoss
                onToggled: root.closeOnFocusLoss = checked
            }
            FormCard.FormDelegateSeparator {}
            FormCard.FormSwitchDelegate {
                text: i18nc("@option:check", "Start in the background at login")
                description: i18n("The first preview after login opens at once. Without this, KwikLook starts when Dolphin opens.")
                checked: root.startAtLogin
                onToggled: root.startAtLogin = checked
            }
        }

        FormCard.FormHeader {
            title: i18nc("@title:group", "Videos and Audio")
        }
        FormCard.FormCard {
            FormCard.FormSwitchDelegate {
                text: i18nc("@option:check", "Start playing when a video or song opens")
                checked: root.autoplayMedia
                onToggled: root.autoplayMedia = checked
            }
            FormCard.FormDelegateSeparator {}
            FormCard.FormSwitchDelegate {
                text: i18nc("@option:check", "Start without sound")
                description: i18n("The sound can be turned on in the preview.")
                checked: root.startMuted
                onToggled: root.startMuted = checked
            }
        }

        FormCard.FormHeader {
            title: i18nc("@title:group", "Office Documents")
        }
        FormCard.FormCard {
            FormCard.FormSwitchDelegate {
                text: i18nc("@option:check", "Preview office documents")
                description: root.officeAvailable
                    ? i18n("Documents, spreadsheets and presentations are converted with LibreOffice, shut away from your files and the network.")
                    : i18n("Needs LibreOffice, which isn't installed.")
                enabled: root.officeAvailable
                checked: root.officePreviews && root.officeAvailable
                onToggled: root.officePreviews = checked
            }
        }

        footer: QQC2.DialogButtonBox {
            standardButtons: QQC2.DialogButtonBox.Ok | QQC2.DialogButtonBox.Apply | QQC2.DialogButtonBox.Cancel
            Component.onCompleted: standardButton(QQC2.DialogButtonBox.Apply).enabled = Qt.binding(() => root.changed)
            onAccepted: if (root.apply()) root.close()
            onApplied: root.apply()
            onRejected: root.close()
        }
    }
}
