// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
import QtQuick

// A frameless window has no resize borders from KWin; these thin strips along
// the edges (wider at the corners) start the compositor's resize.
Item {
    id: handles

    required property Window window
    property int size: 6

    component Edge: MouseArea {
        required property int edges
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        cursorShape: {
            switch (edges) {
            case Qt.LeftEdge:
            case Qt.RightEdge: return Qt.SizeHorCursor
            case Qt.TopEdge:
            case Qt.BottomEdge: return Qt.SizeVerCursor
            case Qt.TopEdge | Qt.LeftEdge:
            case Qt.BottomEdge | Qt.RightEdge: return Qt.SizeFDiagCursor
            default: return Qt.SizeBDiagCursor
            }
        }
        onPressed: handles.window.startSystemResize(edges)
    }

    Edge { edges: Qt.TopEdge; x: handles.size * 2; width: parent.width - handles.size * 4; height: handles.size }
    Edge { edges: Qt.BottomEdge; x: handles.size * 2; y: parent.height - height; width: parent.width - handles.size * 4; height: handles.size }
    Edge { edges: Qt.LeftEdge; y: handles.size * 2; width: handles.size; height: parent.height - handles.size * 4 }
    Edge { edges: Qt.RightEdge; x: parent.width - width; y: handles.size * 2; width: handles.size; height: parent.height - handles.size * 4 }
    Edge { edges: Qt.TopEdge | Qt.LeftEdge; width: handles.size * 2; height: handles.size * 2 }
    Edge { edges: Qt.TopEdge | Qt.RightEdge; x: parent.width - width; width: handles.size * 2; height: handles.size * 2 }
    Edge { edges: Qt.BottomEdge | Qt.LeftEdge; y: parent.height - height; width: handles.size * 2; height: handles.size * 2 }
    Edge { edges: Qt.BottomEdge | Qt.RightEdge; x: parent.width - width; y: parent.height - height; width: handles.size * 2; height: handles.size * 2 }
}
