// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Window
import QtMultimedia
import org.kde.kirigami as Kirigami
import kwiklook

// Videos and audio: the picture (or, for audio, the cover or type icon with title and
// artist) above a standard footer toolbar: play/pause, elapsed time, seek bar, length,
// mute. Plays at once unless turned off in the settings; can start muted. Stops when
// the preview closes or the next file comes. Media keys play and pause; clicking a
// video does too. A file that can't be played shows the fallback view.
ColumnLayout {
    id: view
    objectName: "mediaView"

    // The file being played: cleared on every file change (also reopening the same
    // file), so it loads and starts again.
    property url mediaUrl

    function formatTime(ms) {
        const total = Math.max(0, Math.floor(ms / 1000));
        const hours = Math.floor(total / 3600);
        const minutes = Math.floor(total / 60) % 60;
        const seconds = String(total % 60).padStart(2, "0");
        return hours > 0 ? hours + ":" + String(minutes).padStart(2, "0") + ":" + seconds : minutes + ":" + seconds;
    }
    function togglePlayback() {
        if (player.playbackState === MediaPlayer.PlayingState) {
            player.pause();
        } else {
            player.play();
        }
    }

    spacing: 0
    Component.onCompleted: mediaUrl = Viewer.currentUrl

    Connections {
        target: Viewer
        function onCurrentChanged() {
            player.stop();
            view.mediaUrl = "";
        }
        function onFileInfoChanged() {
            if (Viewer.view === "MediaView" && view.mediaUrl.toString() === "") {
                view.mediaUrl = Viewer.currentUrl;
            }
        }
    }
    // Closing the preview stops the sound.
    Connections {
        target: view.Window.window
        function onVisibleChanged() {
            if (!view.Window.window.visible) {
                player.stop();
            }
        }
    }

    MediaPlayer {
        id: player
        objectName: "mediaPlayer"
        source: view.mediaUrl
        videoOutput: videoOutput
        audioOutput: AudioOutput {
            id: audio
            muted: KwikLookSettings.startMuted
        }
        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.LoadedMedia && KwikLookSettings.autoplayMedia && view.Window.window && view.Window.window.visible) {
                play();
            }
        }
        onSourceChanged: audio.muted = KwikLookSettings.startMuted
    }

    Shortcut {
        sequences: ["Media Play", "Media Pause", "Toggle Media Play/Pause"]
        onActivated: view.togglePlayback()
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        VideoOutput {
            id: videoOutput
            anchors.fill: parent
            visible: player.hasVideo
        }

        // Audio: cover (KDE's thumbnail, as in Dolphin) or the type icon, title, artist.
        ColumnLayout {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            spacing: Kirigami.Units.largeSpacing
            visible: !player.hasVideo

            FileThumbnail {
                Layout.alignment: Qt.AlignHCenter
                size: Math.max(Kirigami.Units.iconSizes.huge, Math.min(view.width - Kirigami.Units.gridUnit * 4, view.height - Kirigami.Units.gridUnit * 10, 384))
                thumbnailEnabled: !player.hasVideo // the cover; a video shows itself
                fallbackIcon: "audio-x-generic"
            }

            Kirigami.Heading {
                Layout.fillWidth: true
                Layout.preferredWidth: 0 // wrapped text: don't widen the layout
                level: 2
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
                text: player.metaData.stringValue(MediaMetaData.Title) || Viewer.fileName
            }
            QQC2.Label {
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: Kirigami.Theme.disabledTextColor
                visible: text !== ""
                text: [player.metaData.stringValue(MediaMetaData.ContributingArtist) || player.metaData.stringValue(MediaMetaData.AlbumArtist),
                       player.metaData.stringValue(MediaMetaData.AlbumTitle)].filter(part => part).join(" · ")
            }
        }

        TapHandler {
            enabled: player.hasVideo
            onTapped: view.togglePlayback()
        }

        // Can't be played (unsupported or damaged).
        Loader {
            anchors.fill: parent
            active: player.error !== MediaPlayer.NoError
            sourceComponent: FallbackView {}
        }
    }

    QQC2.ToolBar {
        Layout.fillWidth: true
        position: QQC2.ToolBar.Footer
        visible: player.error === MediaPlayer.NoError

        RowLayout {
            anchors.fill: parent
            spacing: Kirigami.Units.smallSpacing

            QQC2.ToolButton {
                objectName: "playButton"
                display: QQC2.AbstractButton.IconOnly
                icon.name: player.playbackState === MediaPlayer.PlayingState ? "media-playback-pause" : "media-playback-start"
                text: player.playbackState === MediaPlayer.PlayingState ? i18nc("@action:button", "Pause") : i18nc("@action:button", "Play")
                onClicked: view.togglePlayback()
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
            QQC2.Label {
                text: view.formatTime(seek.pressed ? seek.value : player.position)
                font.features: { "tnum": 1 } // digits of equal width: the label doesn't jitter
            }
            QQC2.Slider {
                id: seek
                objectName: "seekSlider"
                Layout.fillWidth: true
                from: 0
                to: Math.max(1, player.duration)
                enabled: player.seekable
                value: player.position
                onMoved: player.position = value
            }
            QQC2.Label {
                text: view.formatTime(player.duration)
                font.features: { "tnum": 1 }
            }
            QQC2.ToolButton {
                objectName: "muteButton"
                display: QQC2.AbstractButton.IconOnly
                checkable: true
                checked: audio.muted
                icon.name: audio.muted ? "audio-volume-muted" : "audio-volume-high"
                text: audio.muted ? i18nc("@action:button", "Sound On") : i18nc("@action:button", "Mute")
                onToggled: audio.muted = checked
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
        }
    }
}
