// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 KDEGlance contributors
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
// Based on KDEGlance's Dolphin hook (https://github.com/AlexGraveler/KDEGlance).

// Entry point of KwikLook inside Dolphin.
//
// Dolphin loads every "kf6/overlayicon" plugin when it creates a file view
// (before the first window is shown), in every Dolphin process. KwikLook uses
// this plugin type only as a reliable way to be loaded; it never adds overlay
// icons. The work is done by the Bridge, which exists once per Dolphin process.

#include "bridge.h"

#include <KOverlayIconPlugin>

#include <QStringList>
#include <QUrl>

class HookPlugin : public KOverlayIconPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "io.github.strandzen.KwikLook.DolphinHook" FILE "kwiklook_dolphinhook.json")
public:
    explicit HookPlugin(QObject *parent = nullptr)
        : KOverlayIconPlugin(parent)
    {
        Bridge::ensure();
    }

    QStringList getOverlays(const QUrl &item) override
    {
        Q_UNUSED(item)
        return {};
    }
};

#include "hookplugin.moc"
