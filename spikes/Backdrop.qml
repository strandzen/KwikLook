// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick
import kwiklookspike

// Nested session only: a wallpaper behind the test windows, so shadows show.
Window {
    flags: Qt.Window | Qt.FramelessWindowHint
    visibility: Window.Maximized
    color: "#808080"

    Image {
        anchors.fill: parent
        source: SpikeHelper.backdropImage
        sourceSize.width: 1920
        fillMode: Image.PreserveAspectCrop
    }
}
