// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import kwiklook

// Archives (zip, 7z, tar, iso, RAR, …): what's inside, as folders that open and close,
// with size and date; a line above sums it up. Read by a helper locked in a sandbox;
// nothing is unpacked. Big archives show their first 10,000 entries.
ColumnLayout {
    id: view
    objectName: "archiveView"

    spacing: 0
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false

    // Only once the file's details say it's an archive (not the next file's address
    // during a switch, which may be anything).
    function updateSource() {
        if (Viewer.view === "ArchiveView") {
            archive.source = Viewer.currentUrl;
        }
    }
    Component.onCompleted: updateSource()
    Connections {
        target: Viewer
        function onFileInfoChanged() {
            view.updateSource();
        }
    }

    ArchiveModel {
        id: archive
        objectName: "archiveModel"
        onListingChanged: {
            // One folder at the top (the usual): open it.
            if ((status === "ready" || status === "partial") && singleTopFolder()) {
                Qt.callLater(() => tree.expand(0));
            }
        }
    }

    Kirigami.InlineMessage {
        objectName: "archiveMessage"
        Layout.fillWidth: true
        position: Kirigami.InlineMessage.Position.Header
        type: archive.status === "failed" || archive.status === "unsupported" ? Kirigami.MessageType.Warning : Kirigami.MessageType.Information
        visible: text !== ""
        text: archive.status === "partial" && archive.more > 0
                ? i18nc("@info", "Showing the first %1 entries.", Number(10000).toLocaleString(Qt.locale(), "f", 0))
            : archive.status === "partial" ? i18nc("@info", "Part of this archive couldn't be read: showing what could.")
            : archive.status === "locked" ? i18nc("@info", "The names in this archive are protected with a password.")
            : archive.status === "failed" ? i18nc("@info", "This archive couldn't be read.")
            : archive.status === "unsupported" ? i18nc("@info", "This system can't run the archive reader shut away from your files, so archives aren't listed.")
            : ""
    }

    // The summary: counts and sizes, and a lock when contents need a password.
    RowLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing
        visible: archive.summary !== "" || archive.encrypted

        Kirigami.Icon {
            visible: archive.encrypted
            source: "object-locked"
            implicitWidth: Kirigami.Units.iconSizes.small
            implicitHeight: Kirigami.Units.iconSizes.small
        }
        QQC2.Label {
            objectName: "archiveSummary"
            Layout.fillWidth: true
            elide: Text.ElideRight
            color: Kirigami.Theme.disabledTextColor
            text: archive.encrypted && archive.summary !== ""
                ? i18nc("@info summary of an archive, then: protected with a password", "%1 · Password protected", archive.summary)
                : archive.encrypted ? i18nc("@info", "Password protected") : archive.summary
        }
    }

    QQC2.HorizontalHeaderView {
        Layout.fillWidth: true
        syncView: tree
        clip: true
        visible: tree.rows > 0
    }

    QQC2.ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        background: Rectangle {
            color: Kirigami.Theme.backgroundColor
        }

        TreeView {
            id: tree
            objectName: "archiveTree"

            readonly property real sizeWidth: Kirigami.Units.gridUnit * 6
            readonly property real dateWidth: Kirigami.Units.gridUnit * 9

            model: archive
            alternatingRows: true // like Dolphin's details view
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            columnWidthProvider: column => column === 0 ? Math.max(Kirigami.Units.gridUnit * 8, width - sizeWidth - dateWidth)
                : column === 1 ? sizeWidth : dateWidth
            onWidthChanged: forceLayout()

            delegate: QQC2.TreeViewDelegate {
                required property string iconName

                objectName: "archiveEntry"
                // Size and date dimmed, the name in full (the tree's colours: no loop).
                Kirigami.Theme.textColor: column === 0 ? tree.Kirigami.Theme.textColor : tree.Kirigami.Theme.disabledTextColor
                icon.name: column === 0 ? iconName : ""
                icon.width: Kirigami.Units.iconSizes.small
                icon.height: Kirigami.Units.iconSizes.small
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: archive.status === "ready" && tree.rows === 0
            text: i18nc("@info", "This archive is empty")
        }
        DelayedBusyIndicator {
            anchors.centerIn: parent
            busy: archive.status === "reading"
        }
    }
}
