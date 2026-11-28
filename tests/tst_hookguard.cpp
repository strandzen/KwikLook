// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>

// The Dolphin hook's crash guard: a marker on disk around a step's first run,
// a leftover marker = broken under those versions, nothing written once passed.

#include "hookguard.h"

#include <KConfig>
#include <KConfigGroup>

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

class TstHookGuard : public QObject
{
    Q_OBJECT
private:
    QTemporaryDir m_dir;
    QString m_file;

    // As on disk, read with a fresh KConfig.
    QString onDisk(const char *key) const
    {
        return KConfig(m_file, KConfig::SimpleConfig).group(u"DolphinHook"_s).readEntry(key, QString());
    }
    QByteArray fileContents() const
    {
        QFile f(m_file);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }

private Q_SLOTS:
    void init()
    {
        QVERIFY(m_dir.isValid());
        m_file = m_dir.filePath(u"kwiklookstaterc"_s);
        QFile::remove(m_file);
    }

    void firstRunIsMarkedOnDisk()
    {
        HookGuard guard(m_file, u"v1"_s);
        QVERIFY(guard.broken().isEmpty());
        QString during;
        QVERIFY(guard.run(u"read"_s, [&] {
            during = onDisk("Checking");
            return true;
        }));
        QCOMPARE(during, u"read"_s); // written before the step ran
        QVERIFY(onDisk("Checking").isEmpty());
        QCOMPARE(onDisk("Passed"), u"read"_s);
    }

    void passedStepWritesNothing()
    {
        {
            HookGuard guard(m_file, u"v1"_s);
            guard.run(u"read"_s, [] {
                return true;
            });
        }
        HookGuard guard(m_file, u"v1"_s); // a later Dolphin process
        const QByteArray before = fileContents();
        QString during = u"unset"_s;
        guard.run(u"read"_s, [&] {
            during = onDisk("Checking");
            return true;
        });
        QVERIFY(during.isEmpty());
        QCOMPARE(fileContents(), before);
    }

    void incompleteStepIsTriedAgain()
    {
        HookGuard guard(m_file, u"v1"_s);
        QVERIFY(!guard.run(u"read"_s, [] {
            return false; // e.g. an empty folder: nothing to compare yet
        }));
        QVERIFY(onDisk("Checking").isEmpty());
        QVERIFY(onDisk("Passed").isEmpty());
        QString during;
        guard.run(u"read"_s, [&] {
            during = onDisk("Checking");
            return true;
        });
        QCOMPARE(during, u"read"_s);
    }

    void leftoverMarkerMeansBroken()
    {
        {
            // A Dolphin that died during "select": the marker stays.
            KConfig state(m_file, KConfig::SimpleConfig);
            KConfigGroup group = state.group(u"DolphinHook"_s);
            group.writeEntry("Versions", u"v1"_s);
            group.writeEntry("Passed", u"read"_s);
            group.writeEntry("Checking", u"select"_s);
            QVERIFY(state.sync());
        }
        HookGuard guard(m_file, u"v1"_s);
        QVERIFY(guard.broken().contains(u"“select”"_s));
        QVERIFY(onDisk("Checking").isEmpty());
        // And it stays broken for later Dolphin processes.
        QVERIFY(!HookGuard(m_file, u"v1"_s).broken().isEmpty());
    }

    void newVersionsStartAfresh()
    {
        {
            KConfig state(m_file, KConfig::SimpleConfig);
            KConfigGroup group = state.group(u"DolphinHook"_s);
            group.writeEntry("Versions", u"v1"_s);
            group.writeEntry("Passed", u"read"_s);
            group.writeEntry("Broken", u"earlier crash"_s);
            QVERIFY(state.sync());
        }
        HookGuard guard(m_file, u"v2"_s);
        QVERIFY(guard.broken().isEmpty());
        QVERIFY(onDisk("Broken").isEmpty());
        QVERIFY(onDisk("Passed").isEmpty());
        QCOMPARE(onDisk("Versions"), u"v2"_s);
    }

    void otherGroupsAreLeftAlone()
    {
        {
            KConfig state(m_file, KConfig::SimpleConfig);
            state.group(u"Viewer"_s).writeEntry("InfoOpen", true);
            QVERIFY(state.sync());
        }
        HookGuard guard(m_file, u"v1"_s);
        guard.run(u"read"_s, [] {
            return true;
        });
        QCOMPARE(KConfig(m_file, KConfig::SimpleConfig).group(u"Viewer"_s).readEntry("InfoOpen", false), true);
    }
};

QTEST_GUILESS_MAIN(TstHookGuard)
#include "tst_hookguard.moc"
