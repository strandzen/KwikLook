// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import kwiklook

// Contacts (.vcf) and events (.ics) as cards, one below the other: a contact with its
// photo (or initials), name, organization, then phones, mail, addresses, birthday;
// an event with its title, when, then place, organizer, repeats, description. The
// bar's view toggle shows the file's text instead.
QQC2.ScrollView {
    id: view
    objectName: "cardsView"

    // A date (all day) or date and time, as the user's locale writes them.
    function when(card) {
        const start = new Date(card.start);
        const end = new Date(card.end);
        if (card.allDay) {
            const first = start.toLocaleDateString(Qt.locale(), Locale.LongFormat);
            return end > start && end.toDateString() !== start.toDateString()
                ? i18nc("@info from date to date", "%1 – %2", first, end.toLocaleDateString(Qt.locale(), Locale.LongFormat)) : first;
        }
        const from = start.toLocaleString(Qt.locale(), Locale.ShortFormat);
        if (!(end > start)) {
            return from;
        }
        const to = end.toDateString() === start.toDateString() ? end.toLocaleTimeString(Qt.locale(), Locale.ShortFormat)
                                                               : end.toLocaleString(Qt.locale(), Locale.ShortFormat);
        return i18nc("@info from time to time", "%1 – %2", from, to);
    }

    // A label and its value, the label right-aligned in a fixed column.
    component Row: RowLayout {
        property string label
        property string value

        Layout.fillWidth: true
        visible: value !== ""
        spacing: Kirigami.Units.largeSpacing

        QQC2.Label {
            Layout.preferredWidth: Kirigami.Units.gridUnit * 7
            Layout.alignment: Qt.AlignTop
            horizontalAlignment: Text.AlignRight
            color: Kirigami.Theme.disabledTextColor
            elide: Text.ElideRight
            text: parent.label
        }
        QQC2.Label {
            Layout.fillWidth: true
            Layout.preferredWidth: 0 // wrapped text: don't widen the layout
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            text: parent.value
        }
    }

    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false
    QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

    background: Rectangle {
        color: Kirigami.Theme.backgroundColor
    }

    Item {
        implicitWidth: view.availableWidth
        implicitHeight: column.implicitHeight + Kirigami.Units.gridUnit * 2

        ColumnLayout {
            id: column
            anchors.horizontalCenter: parent.horizontalCenter
            y: Math.max(Kirigami.Units.gridUnit, (view.availableHeight - implicitHeight) / 2)
            width: Math.min(parent.width - Kirigami.Units.gridUnit * 2, Kirigami.Units.gridUnit * 28)
            spacing: Kirigami.Units.gridUnit

            Kirigami.PlaceholderMessage {
                Layout.fillWidth: true
                visible: Viewer.cards.length === 0
                icon.name: Viewer.formattedKind === "calendar" ? "view-calendar" : "contact-new"
                text: Viewer.formattedKind === "calendar" ? i18nc("@info", "No events in this file") : i18nc("@info", "No contacts in this file")
            }

            Repeater {
                model: Viewer.cards

                ColumnLayout {
                    id: card
                    objectName: "card"
                    required property var modelData
                    required property int index
                    readonly property bool contact: modelData.kind === "contact"

                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Separator {
                        Layout.fillWidth: true
                        Layout.bottomMargin: Kirigami.Units.gridUnit
                        visible: card.index > 0
                    }
                    Kirigami.Avatar {
                        Layout.alignment: Qt.AlignHCenter
                        implicitWidth: Kirigami.Units.iconSizes.huge
                        implicitHeight: implicitWidth
                        visible: card.contact
                        name: card.modelData.name || ""
                        source: card.modelData.photo || ""
                        // Kirigami's own default for this is broken ("Unable to assign double to QSizeF").
                        sourceSize: Qt.size(width, height)
                    }
                    Kirigami.Icon {
                        Layout.alignment: Qt.AlignHCenter
                        implicitWidth: Kirigami.Units.iconSizes.huge
                        implicitHeight: implicitWidth
                        visible: !card.contact
                        source: "view-calendar-day"
                    }
                    Kirigami.Heading {
                        objectName: "cardTitle"
                        Layout.fillWidth: true
                        Layout.preferredWidth: 0
                        level: 2
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        text: (card.contact ? card.modelData.name : card.modelData.summary)
                            || (card.contact ? i18nc("@info a contact without a name", "No name") : i18nc("@info an event without a title", "No title"))
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 0
                        Layout.bottomMargin: Kirigami.Units.largeSpacing
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        color: Kirigami.Theme.disabledTextColor
                        visible: text !== ""
                        text: card.contact ? card.modelData.organization : view.when(card.modelData)
                    }

                    // Contact details.
                    Repeater {
                        model: card.contact ? card.modelData.phones : []
                        delegate: Row {
                            required property var modelData
                            label: modelData.label || i18nc("@label", "Phone")
                            value: modelData.value
                        }
                    }
                    Repeater {
                        model: card.contact ? card.modelData.emails : []
                        delegate: Row {
                            required property string modelData
                            label: i18nc("@label", "Email")
                            value: modelData
                        }
                    }
                    Repeater {
                        model: card.contact ? card.modelData.addresses : []
                        delegate: Row {
                            required property string modelData
                            label: i18nc("@label", "Address")
                            value: modelData
                        }
                    }
                    Row {
                        label: i18nc("@label", "Birthday")
                        value: card.contact && card.modelData.birthday ? new Date(card.modelData.birthday).toLocaleDateString(Qt.locale(), Locale.LongFormat) : ""
                    }
                    Row {
                        label: i18nc("@label", "Website")
                        value: card.contact ? card.modelData.url : ""
                    }
                    Row {
                        label: i18nc("@label", "Note")
                        value: card.contact ? card.modelData.note : ""
                    }

                    // Event details.
                    Row {
                        label: i18nc("@label", "Place")
                        value: card.contact ? "" : card.modelData.location
                    }
                    Row {
                        label: i18nc("@label", "Organizer")
                        value: card.contact ? "" : card.modelData.organizer
                    }
                    Row {
                        label: i18nc("@label", "Repeats")
                        value: !card.contact && card.modelData.repeats ? i18nc("@info the event repeats", "Yes") : ""
                    }
                    Row {
                        label: i18nc("@label", "Description")
                        value: card.contact ? "" : card.modelData.description
                    }
                }
            }

            QQC2.Label {
                Layout.alignment: Qt.AlignHCenter
                visible: Viewer.moreCards > 0
                color: Kirigami.Theme.disabledTextColor
                text: i18ncp("@info", "And one more", "And %1 more", Viewer.moreCards)
            }
        }
    }
}
