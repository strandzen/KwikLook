// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

/*
 * “Start in the background at login”: an entry in ~/.config/autostart that runs
 * `kwiklook --service`. The file itself is the setting (it also shows in System
 * Settings → Autostart, where it can be removed too).
 *
 *   Autostart { id: autostart }   // autostart.enabled, autostart.setEnabled(true)
 */
class Autostart : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool enabled READ isEnabled NOTIFY enabledChanged)

public:
    explicit Autostart(QObject *parent = nullptr);

    static QString filePath();
    bool isEnabled() const;
    // Writes or removes the entry; false (and a warning in the log) if that failed.
    Q_INVOKABLE bool setEnabled(bool enabled);

Q_SIGNALS:
    void enabledChanged();
};
