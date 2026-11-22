// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
pragma Singleton

import QtQuick

// What can be put in the preview's bar (TitleBar.qml), by the names kept in the
// settings (BarLeft, BarRight): icon and name for the bar and the settings window.
// "Spacer" stretches and can be used more than once. "ViewMode" (formatted / source)
// shows only for files that can show both ways (Viewer.formattedKind), "Pages" only
// for PDFs with more than one page.
QtObject {
    readonly property list<string> buttons: ["Previous", "Next", "Pages", "ViewMode", "Information", "Share", "OpenWith", "Open", "ShowInDolphin", "CopyPath", "Close"]

    function iconName(item) {
        switch (item) {
        case "Previous": return "go-previous";
        case "Next": return "go-next";
        case "Pages": return "sidebar-expand-left";
        case "ViewMode": return "view-preview";
        case "Information": return "documentinfo";
        case "Share": return "document-share";
        case "OpenWith": return "document-open";
        case "Open": return "quickopen";
        case "ShowInDolphin": return "system-file-manager";
        case "CopyPath": return "edit-copy-path";
        case "Close": return "window-close";
        case "Spacer": return "distribute-horizontal-x";
        }
        return "";
    }

    function text(item) {
        switch (item) {
        case "Previous": return i18nc("@action:button", "Previous File");
        case "Next": return i18nc("@action:button", "Next File");
        case "Pages": return i18nc("@action:button sidebar with small pages of a PDF", "Page Overview");
        case "ViewMode": return i18nc("@action:button show a file formatted (e.g. Markdown), not as source", "Formatted View");
        case "Information": return i18nc("@action:button", "Information");
        case "Share": return i18nc("@action:button", "Share");
        case "OpenWith": return i18nc("@action:button", "Open With…");
        case "Open": return i18nc("@action:button open the file in its usual application", "Open");
        case "ShowInDolphin": return i18nc("@action:button", "Show in Dolphin");
        case "CopyPath": return i18nc("@action:button", "Copy Path");
        case "Close": return i18nc("@action:button", "Close");
        case "Spacer": return i18nc("@item a gap in the bar that stretches", "Flexible Space");
        }
        return "";
    }
}
