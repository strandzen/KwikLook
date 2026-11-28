// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <KSharedConfig>

#include <QString>
#include <QStringList>

/*
 * Keeps a Dolphin update from turning into a crash on every Space.
 *
 * The first time a step that calls Dolphin's internals runs under a given
 * Dolphin and KwikLook version ("read", "select"), a marker goes to disk first
 * and is removed when the step returns. A marker still there when the next
 * Dolphin starts means Dolphin died inside that step: the internals are not
 * used again under these versions (`broken()`). Once a step has passed, it runs
 * without writing anything. A new Dolphin or KwikLook version starts afresh.
 *
 * Kept in the [DolphinHook] group of a state file (kwiklookstaterc).
 */
class HookGuard
{
public:
    // `file`: absolute path; `versions`: see versionsKey().
    HookGuard(const QString &file, const QString &versions);

    // $XDG_STATE_HOME/kwiklookstaterc (an absolute path: inside Dolphin,
    // openStateConfig() would mean Dolphin's own file).
    static QString defaultFile();
    static QString versionsKey(const QString &dolphinVersion);

    // Why the internals must not be used under these versions, or empty.
    QString broken() const;

    // Runs `step` (returns true when it got to check everything). Marked on
    // disk around it until it has once returned true under these versions.
    template<typename Step>
    bool run(const QString &name, Step &&step)
    {
        const bool marked = begin(name);
        const bool done = step();
        if (marked) {
            end(name, done);
        }
        return done;
    }

private:
    bool begin(const QString &name);
    void end(const QString &name, bool done);

    KSharedConfig::Ptr m_config;
    QString m_broken;
    QStringList m_passed;
};
