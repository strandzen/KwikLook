// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

// D-Bus names shared by the viewer (the app) and the Dolphin hook.
namespace KwikLookDBus
{
constexpr auto Service = "io.github.strandzen.KwikLook";
constexpr auto ViewerPath = "/Viewer";
constexpr auto ViewerInterface = "io.github.strandzen.KwikLook.Viewer";
// Diagnostics of the hook inside each Dolphin process (Dolphin's own bus name).
constexpr auto BridgePath = "/KwikLookBridge";
}
