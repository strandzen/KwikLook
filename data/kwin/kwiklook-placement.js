// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
//
// KwikLook's KWin script: places the preview window where the settings say and gives
// it each picture's shape, centred. On Wayland only KWin can move windows. The viewer
// loads this file at runtime over KWin's D-Bus /Scripting (src/gui/kwinscript.cpp) and
// unloads it on quit; nothing is installed into KWin.
//
// The script asks the viewer what to do: Placement(caption) on its D-Bus interface,
// answered as JSON (see Viewer::Placement). It asks when the preview window appears
// and when its title changes (the title is the file name: a new file).
//
// The functions before the KWin part are plain JavaScript: tests/tst_placement.cpp
// loads this file into a QJSEngine (no `workspace` there) and calls them.

/* exported fitSize, placeIn, centreOf */

const PlacementShare = 0.8; // a picture's window covers at most this much of the area

// The window size that shows `answer.picture` (logical pixels) at real size, or smaller
// to fit `area`: the picture scaled down, never up, keeping its shape; `answer.chrome`
// is the room around it (bar, margins, information panel). Never below the window's
// minimum size.
function fitSize(answer, area) {
    const pic = answer.picture;
    const chrome = answer.chrome;
    const scale = Math.min(1,
        (area.width * PlacementShare - chrome.width) / pic.width,
        (area.height * PlacementShare - chrome.height) / pic.height);
    return {
        width: Math.round(Math.max(answer.minimum.width, pic.width * scale + chrome.width)),
        height: Math.round(Math.max(answer.minimum.height, pic.height * scale + chrome.height)),
    };
}

// `size` centred on `centre`, moved (and if need be shrunk) to lie inside `area`.
function placeIn(size, centre, area) {
    const width = Math.min(size.width, area.width);
    const height = Math.min(size.height, area.height);
    const x = Math.max(area.x, Math.min(centre.x - width / 2, area.x + area.width - width));
    const y = Math.max(area.y, Math.min(centre.y - height / 2, area.y + area.height - height));
    return { x: Math.round(x), y: Math.round(y), width: Math.round(width), height: Math.round(height) };
}

function centreOf(rect) {
    return { x: rect.x + rect.width / 2, y: rect.y + rect.height / 2 };
}

if (typeof workspace !== "undefined") {
    const Service = "io.github.strandzen.KwikLook";
    const AppId = "io.github.strandzen.KwikLook";
    const UndoWindowMs = 1000;

    let lastOther = null; // the last other window focused before the preview
    // "Centred on Dolphin": the Dolphin window the preview came from, if it did.
    const dolphin = function () {
        return lastOther && lastOther.desktopFileName === "org.kde.dolphin" ? lastOther : null;
    };
    let normalSize = null; // the window's size before pictures reshaped it; null while not reshaped

    const areaFor = function (window, answer, opening) {
        if (opening && answer.mode === "pointer") {
            return workspace.clientArea(KWin.PlacementArea, workspace.screenAt(workspace.cursorPos), workspace.currentDesktop);
        }
        if (opening && answer.mode === "dolphin" && dolphin()) {
            return workspace.clientArea(KWin.PlacementArea, dolphin());
        }
        return workspace.clientArea(KWin.PlacementArea, window);
    };

    const place = function (window, answer, opening) {
        if (window.deleted || window.fullScreen || window.kwiklookHandSized) {
            return; // a size set by hand stays until the preview closes
        }
        const area = areaFor(window, answer, opening);
        const frame = window.frameGeometry;
        let size = { width: frame.width, height: frame.height };
        if (answer.picture) {
            if (!normalSize) {
                normalSize = size;
            }
            size = fitSize(answer, area);
        } else if (normalSize) {
            size = normalSize;
            normalSize = null;
        }
        let centre = centreOf(frame); // a new file: reshaped around the centre
        if (opening) {
            if (answer.mode === "pointer") {
                centre = workspace.cursorPos;
            } else if (answer.mode === "dolphin" && dolphin()) {
                centre = centreOf(dolphin().frameGeometry);
            } else {
                centre = centreOf(area);
            }
        }
        const target = placeIn(size, centre, area);
        // KWin can undo a move made right after the window appeared (its own first
        // placement lands later): place it once more if that happens. Not after a later
        // placement of ours (a new file), which isn't KWin's to undo.
        window.kwiklookTarget = opening ? target : null;
        window.kwiklookPlacedAt = Date.now();
        window.frameGeometry = target;
    };

    const ask = function (window, opening) {
        callDBus(Service, "/Viewer", "io.github.strandzen.KwikLook.Viewer", "Placement", window.captionNormal, function (reply) {
            let answer;
            try {
                answer = JSON.parse(reply);
            } catch (e) {
                return; // the viewer went away, or an older one
            }
            if (answer.preview) {
                place(window, answer, opening);
            }
        });
    };

    const watch = function (window) {
        if (window.desktopFileName !== AppId || !window.normalWindow) {
            return;
        }
        window.frameGeometryChanged.connect(function () {
            const target = window.kwiklookTarget;
            if (!target || Date.now() - window.kwiklookPlacedAt > UndoWindowMs) {
                return;
            }
            const frame = window.frameGeometry;
            if (Math.round(frame.x) !== target.x || Math.round(frame.y) !== target.y) {
                window.kwiklookTarget = null; // once
                window.frameGeometry = target;
            }
        });
        window.interactiveMoveResizeFinished.connect(function () {
            if (window.kwiklookResizing) {
                window.kwiklookHandSized = true;
                normalSize = null; // the size set by hand is the one to keep
            }
        });
        window.moveResizedChanged.connect(function () {
            if (window.resize) {
                window.kwiklookResizing = true;
            }
        });
        window.captionChanged.connect(function () {
            ask(window, false);
        });
        ask(window, true);
    };

    workspace.windowAdded.connect(watch);
    workspace.windowActivated.connect(function (window) {
        if (window && window.normalWindow && window.desktopFileName !== AppId) {
            lastOther = window;
        }
    });
    workspace.windowRemoved.connect(function (window) {
        if (window === lastOther) {
            lastOther = null;
        }
    });
}
