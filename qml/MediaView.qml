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
// repeat, mute, volume, fullscreen. Plays at once unless turned off in the settings;
// can start muted. Stops when the preview closes or the next file comes. Media keys
// play and pause; clicking a video does too. Fullscreen (button, double-click, F) shows
// only the video, the controls appear while the mouse moves. Edit (videos, with
// ffmpeg) trims and rotates, saved as a copy next to the original (VideoEditor): a
// start on a keyframe (ticks on the slider; a handle let go near one moves onto it)
// is a quick, lossless cut; anywhere else the video is re-encoded to start exactly
// there. A file that can't be played shows the fallback view.
Item {
    id: view
    objectName: "mediaView"

    readonly property bool fullScreen: Window.window !== null && Window.window.visibility === Window.FullScreen
    // Fullscreen: the controls show while the mouse moves (or is over them).
    property bool controlsShown: true
    // Trimming and rotating: the edit bar is open; quarter turns counterclockwise.
    property bool editing: false
    property int turn: 0
    // The start chosen gives a quick, lossless cut (re-read when the keyframes arrive).
    readonly property bool quickCut: {
        editor.keyframes;
        return editor.isQuickCut(range.first.value / 1000) || !editor.exactAvailable;
    }

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
    function startEditing() {
        if (fullScreen) {
            view.Window.window.showNormal();
        }
        turn = 0;
        range.first.value = 0;
        range.second.value = player.duration;
        editing = true;
        editor.probe();
    }
    function stopEditing() {
        editor.cancel();
        editing = false;
        turn = 0;
    }
    function notify(text) {
        const window = view.QQC2.ApplicationWindow.window;
        if (window) {
            window.showPassiveNotification(text);
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
            view.stopEditing();
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

    VideoEditor {
        id: editor
        source: view.mediaUrl
        onSaved: copy => {
            view.stopEditing();
            view.notify(i18nc("@info", "Saved as “%1”.", decodeURIComponent(copy.toString().split("/").pop())));
        }
        onFailed: message => view.notify(message)
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
            }
        }
        onSourceChanged: audio.muted = KwikLookSettings.startMuted
        // While editing, playing stays within the chosen part.
        onPositionChanged: {
            if (view.editing && playbackState === MediaPlayer.PlayingState && position >= range.second.value) {
                pause();
                position = range.first.value;
            }
        }
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
        anchors.bottom: view.fullScreen ? parent.bottom : view.editing ? editBar.top : footer.top

        VideoOutput {
            id: videoOutput
            anchors.fill: parent
            visible: player.hasVideo
            // The turn being edited, before it's saved. Qt turns clockwise, ffmpeg's
            // rotation (what is saved) counterclockwise: checked side by side.
            orientation: (360 - view.turn) % 360
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

    // Trim and rotate: which part to keep (the start snaps to keyframes), quarter turns,
    // Save Copy, Cancel.
    QQC2.ToolBar {
        id: editBar
        objectName: "editBar"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: footer.top
        position: QQC2.ToolBar.Footer
        visible: view.editing && !view.fullScreen

        RowLayout {
            anchors.fill: parent
            spacing: Kirigami.Units.smallSpacing

            QQC2.ToolButton {
                objectName: "rotateLeft"
                display: QQC2.AbstractButton.IconOnly
                icon.name: "object-rotate-left"
                text: i18nc("@action:button", "Rotate Left")
                enabled: !editor.saving
                onClicked: view.turn = (view.turn + 90) % 360
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
            QQC2.ToolButton {
                objectName: "rotateRight"
                display: QQC2.AbstractButton.IconOnly
                icon.name: "object-rotate-right"
                text: i18nc("@action:button", "Rotate Right")
                enabled: !editor.saving
                onClicked: view.turn = (view.turn + 270) % 360
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
            QQC2.RangeSlider {
                id: range
                objectName: "trimRange"

                // Let go near a keyframe: onto it, for a quick cut.
                function magnet() {
                    const keyframe = editor.keyframeNear(first.value / 1000, Math.max(0.3, player.duration / 1000 * 0.015));
                    if (keyframe >= 0) {
                        first.value = keyframe * 1000;
                        player.position = first.value;
                    }
                }

                Layout.fillWidth: true
                from: 0
                to: Math.max(1, player.duration)
                visible: !(editor.saving && editor.progress >= 0)
                enabled: !editor.probing && !editor.saving
                first.onMoved: {
                    // Without an encoder for exact cuts, the start can only be on a keyframe.
                    if (!editor.exactAvailable) {
                        first.value = editor.keyframeAtOrBefore(first.value / 1000) * 1000;
                    }
                    player.position = first.value;
                }
                first.onPressedChanged: if (!first.pressed && editor.exactAvailable) magnet()
                second.onMoved: player.position = second.value

                // Keyframes: where a quick cut can start.
                Repeater {
                    model: editor.keyframes.length <= 400 ? editor.keyframes : []
                    delegate: Rectangle {
                        required property real modelData
                        x: range.leftPadding + range.first.handle.width / 2 - width / 2
                           + modelData * 1000 / range.to * (range.availableWidth - range.first.handle.width)
                        y: range.topPadding + range.availableHeight / 2 + range.first.handle.height / 2
                        width: 1
                        height: Kirigami.Units.smallSpacing
                        color: Kirigami.Theme.disabledTextColor
                    }
                }
            }
            QQC2.ProgressBar {
                objectName: "saveProgress"
                Layout.fillWidth: true
                visible: editor.saving && editor.progress >= 0
                value: editor.progress
            }
            QQC2.Label {
                font.features: { "tnum": 1 }
                text: i18nc("@info part of a video, from – to", "%1 – %2", view.formatTime(range.first.value), view.formatTime(range.second.value))
            }
            // Which kind of cut the start gives.
            QQC2.Label {
                objectName: "cutKind"
                visible: !editor.probing && !editor.saving
                color: Kirigami.Theme.disabledTextColor
                text: view.quickCut
                    ? i18nc("@info a lossless trim", "Quick") : i18nc("@info a trim that re-encodes the video", "Exact, slower")
                QQC2.ToolTip.text: view.quickCut
                    ? i18nc("@info:tooltip", "The start is at a keyframe (the small marks): the video is copied as it is, at once and without losing quality.")
                    : i18nc("@info:tooltip", "The start is between keyframes (the small marks): the video is re-encoded to start exactly there. Takes longer; the picture stays very close to the original.")
                QQC2.ToolTip.visible: kindHover.hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                HoverHandler {
                    id: kindHover
                }
            }
            QQC2.BusyIndicator {
                Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                Layout.preferredWidth: Layout.preferredHeight
                visible: editor.probing || (editor.saving && editor.progress < 0)
                running: visible
            }
            QQC2.Button {
                objectName: "saveCopy"
                icon.name: "document-save-as"
                text: i18nc("@action:button", "Save Copy")
                enabled: !editor.probing && !editor.saving
                    && (view.turn !== 0 || range.first.value > 0 || range.second.value < player.duration)
                onClicked: editor.save(range.first.value / 1000,
                                       range.second.value >= player.duration ? 0 : range.second.value / 1000, view.turn, player.duration / 1000)
                QQC2.ToolTip.text: i18nc("@info:tooltip", "Saves the part and turn as a new file next to this one; this video stays as it is.")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
            QQC2.ToolButton {
                display: QQC2.AbstractButton.IconOnly
                icon.name: "dialog-cancel"
                text: i18nc("@action:button stop editing the video", "Cancel")
                onClicked: view.stopEditing()
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
        }
    }

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
                objectName: "editButton"
                display: QQC2.AbstractButton.IconOnly
                visible: player.hasVideo && editor.available && Viewer.currentUrl.toString().startsWith("file:") && !view.fullScreen
                checkable: true
                checked: view.editing
                icon.name: "edit-cut"
                text: i18nc("@action:button", "Trim and Rotate")
                onClicked: view.editing ? view.stopEditing() : view.startEditing()
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
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
