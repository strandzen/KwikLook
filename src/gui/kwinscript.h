// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QObject>
#include <QString>

/*
 * KwikLook's KWin script (data/kwin/kwiklook-placement.js): loaded into the running
 * KWin over its D-Bus /Scripting interface, unloaded again on quit or when the window
 * position setting says “Let KWin decide”. Nothing is installed into KWin and no KWin
 * setting changes. Without KWin (or with scripting turned off) loading fails and the
 * preview behaves as before.
 */
class KWinScript : public QObject
{
    Q_OBJECT

public:
    static constexpr auto PluginName = "kwiklook-placement";

    explicit KWinScript(const QString &path, QObject *parent = nullptr);

    // The script file: next to the binary (build tree), else the installed copy.
    static QString defaultPath();

    // Loads or unloads; a copy left loaded by a viewer that crashed is replaced.
    // Returns whether the script is loaded afterwards.
    bool setLoaded(bool loaded);

private:
    QString m_path;
    bool m_loaded = false;
};
