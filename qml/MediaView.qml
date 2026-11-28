// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Effects
import QtMultimedia
import org.kde.kirigami as Kirigami
import kwiklook

// Videos and audio: the picture (or, for audio, the cover or type icon with title and
// artist) above a standard footer toolbar: play/pause, elapsed time, seek bar, length,
// repeat, mute, volume, fullscreen. Plays at once unless turned off in the settings;
// can start muted. Stops when the preview closes or the next file comes. Media keys
// play and pause; clicking a video does too. Fullscreen (button, double-click, F) shows
// only the video, the controls appear while the mouse moves. Without autoplay a video
// shows its first frame. A file that can't be played shows the fallback view.
Item {
    id: view
    objectName: "mediaView"

    readonly property bool fullScreen: Window.window !== null && Window.window.visibility === Window.FullScreen
    // Fullscreen: the controls show while the mouse moves (or is over them).
    property bool controlsShown: true
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
    function toggleFullScreen() {
        const window = view.Window.window;
        if (fullScreen) {
            window.showNormal();
        } else if (player.hasVideo) {
            window.showFullScreen();
        }
    }
    function showControls() {
        controlsShown = true;
        hideControlsTimer.restart();
    }
    function togglePlayback() {
        if (player.playbackState === MediaPlayer.PlayingState) {
            player.pause();
        } else {
            player.play();
        }
    }

    Component.onCompleted: mediaUrl = Viewer.currentUrl
    onFullScreenChanged: showControls()

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
        loops: Viewer.mediaRepeat ? MediaPlayer.Infinite : 1
        audioOutput: AudioOutput {
            id: audio
            muted: KwikLookSettings.startMuted
            // The slider shows perceived loudness; cubic, as PulseAudio and PipeWire map it.
            volume: Math.pow(Viewer.mediaVolume, 3)
        }
        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.LoadedMedia && KwikLookSettings.autoplayMedia && view.Window.window && view.Window.window.visible) {
                play();
            } else if (mediaStatus === MediaPlayer.LoadedMedia && hasVideo && playbackState === MediaPlayer.StoppedState) {
                pause(); // shows the first frame instead of black
            }
        }
        onSourceChanged: audio.muted = KwikLookSettings.startMuted
    }

    Shortcut {
        sequences: ["Media Play", "Media Pause", "Toggle Media Play/Pause"]
        onActivated: view.togglePlayback()
    }
    Shortcut {
        sequence: "F"
        enabled: player.hasVideo
        onActivated: view.toggleFullScreen()
    }
    Timer {
        id: hideControlsTimer
        interval: 2000
        onTriggered: view.controlsShown = !view.fullScreen || controlsHover.hovered
    }

    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        // Fullscreen: the video takes the whole screen, the controls float over it.
        anchors.bottom: view.fullScreen ? parent.bottom : footer.top

        VideoOutput {
            id: videoOutput
            anchors.fill: parent
            visible: player.hasVideo
        }

        // Audio: the cover, blurred and washed with the background colour, fills the area
        // behind (like Elisa). A still picture: no cost once drawn.
        Item {
            anchors.fill: parent
            visible: !player.hasVideo && cover.hasThumbnail
            clip: true

            ShaderEffectSource {
                id: coverTexture
                sourceItem: cover.hasThumbnail ? cover.thumbnailItem : null
                hideSource: false
                visible: false
            }
            MultiEffect {
                // Covers the area, cropped (the cover's proportions kept).
                readonly property real ratio: cover.thumbnailItem.paintedWidth / Math.max(1, cover.thumbnailItem.paintedHeight)
                anchors.centerIn: parent
                width: Math.max(parent.width, parent.height * ratio) * 1.1 // the blur's soft edge outside
                height: width / Math.max(0.01, ratio)
                source: coverTexture
                autoPaddingEnabled: false
                blurEnabled: true
                blurMax: 64
                blur: 1
                saturation: 0.2
            }
            Rectangle {
                anchors.fill: parent
                color: Kirigami.Theme.backgroundColor
                opacity: 0.6
            }
        }

        // Audio: cover (KDE's thumbnail, as in Dolphin) or the type icon, title, artist.
        ColumnLayout {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            spacing: Kirigami.Units.largeSpacing
            visible: !player.hasVideo

            FileThumbnail {
                id: cover
                objectName: "cover"
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

        // Not playing: a large play button over the frame.
        QQC2.RoundButton {
            objectName: "bigPlayButton"
            anchors.centerIn: parent
            visible: player.hasVideo && player.playbackState !== MediaPlayer.PlayingState && player.error === MediaPlayer.NoError
            width: Kirigami.Units.gridUnit * 4
            height: width
            icon.name: "media-playback-start"
            icon.width: Kirigami.Units.iconSizes.large
            icon.height: Kirigami.Units.iconSizes.large
            QQC2.ToolTip.text: i18nc("@action:button", "Play")
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            onClicked: view.togglePlayback()
        }

        TapHandler {
            enabled: player.hasVideo
            onTapped: view.togglePlayback()
            onDoubleTapped: view.toggleFullScreen()
        }
        HoverHandler {
            enabled: view.fullScreen
            cursorShape: view.controlsShown ? Qt.ArrowCursor : Qt.BlankCursor
            onPointChanged: view.showControls()
        }

        // Can't be played (unsupported or damaged).
        Loader {
            anchors.fill: parent
            active: player.error !== MediaPlayer.NoError
            sourceComponent: FallbackView {}
        }
    }

    // The controls' height, for floating notes in the corner to keep above them.
    readonly property real bottomBarHeight: footer.visible ? footer.height : 0

    QQC2.ToolBar {
        id: footer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        position: QQC2.ToolBar.Footer
        opacity: player.error === MediaPlayer.NoError && (!view.fullScreen || view.controlsShown) ? 1 : 0
        visible: opacity > 0
        Behavior on opacity {
            NumberAnimation {
                duration: Kirigami.Units.longDuration
            }
        }

        HoverHandler {
            id: controlsHover
        }

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
                objectName: "repeatButton"
                display: QQC2.AbstractButton.IconOnly
                checkable: true
                checked: Viewer.mediaRepeat
                icon.name: "media-playlist-repeat"
                text: i18nc("@action:button", "Repeat")
                onToggled: Viewer.mediaRepeat = checked
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
            QQC2.ToolButton {
                objectName: "muteButton"
                display: QQC2.AbstractButton.IconOnly
                checkable: true
                checked: audio.muted
                icon.name: audio.muted || Viewer.mediaVolume === 0 ? "audio-volume-muted"
                    : Viewer.mediaVolume < 0.34 ? "audio-volume-low"
                    : Viewer.mediaVolume < 0.67 ? "audio-volume-medium" : "audio-volume-high"
                text: audio.muted ? i18nc("@action:button", "Sound On") : i18nc("@action:button", "Mute")
                onToggled: audio.muted = checked
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
            QQC2.Slider {
                objectName: "volumeSlider"
                Layout.preferredWidth: Kirigami.Units.gridUnit * 5
                from: 0
                to: 1
                value: Viewer.mediaVolume
                onMoved: {
                    Viewer.mediaVolume = value;
                    audio.muted = false;
                }
                QQC2.ToolTip.text: i18nc("@info:tooltip volume in percent", "Volume: %1%", Math.round(value * 100))
                QQC2.ToolTip.visible: hovered || pressed
                QQC2.ToolTip.delay: pressed ? 0 : Kirigami.Units.toolTipDelay
            }
            QQC2.ToolButton {
                objectName: "fullScreenButton"
                display: QQC2.AbstractButton.IconOnly
                visible: player.hasVideo
                icon.name: view.fullScreen ? "view-restore" : "view-fullscreen"
                text: view.fullScreen ? i18nc("@action:button", "Exit Full Screen") : i18nc("@action:button", "Full Screen")
                onClicked: view.toggleFullScreen()
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
        }
    }
}
