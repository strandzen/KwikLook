// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>

// SandboxRun: one program in kwiklook-sandbox with a time limit, as the office,
// book and archive previews run their helpers.

#include "sandbox.h"

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;

class TstSandboxRun : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        if (Sandbox::binary().isEmpty()) {
            QFAIL("kwiklook-sandbox not found next to the test");
        }
        if (!Sandbox::supported()) {
            QSKIP("this kernel's Landlock can't lock a program in (ABI 6 and RESOLVE_UNIX needed)");
        }
    }

    void outputArrives()
    {
        SandboxRun run(5000, this);
        QSignalSpy finished(&run, &SandboxRun::finished);
        run.start({u"--"_s, u"echo"_s, u"hello"_s});
        QVERIFY(run.isRunning());
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.at(0).at(0).value<SandboxRun::End>(), SandboxRun::End::Exited);
        QCOMPARE(finished.at(0).at(1).toInt(), 0);
        QCOMPARE(finished.at(0).at(2).toByteArray(), "hello\n");
        QVERIFY(!run.isRunning());
    }

    void exitCodeComesThrough()
    {
        SandboxRun run(5000, this);
        QSignalSpy finished(&run, &SandboxRun::finished);
        run.start({u"--"_s, u"sh"_s, u"-c"_s, u"exit 3"_s});
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.at(0).at(0).value<SandboxRun::End>(), SandboxRun::End::Exited);
        QCOMPARE(finished.at(0).at(1).toInt(), 3);
    }

    void timeLimitStopsIt()
    {
        SandboxRun run(200, this);
        QSignalSpy finished(&run, &SandboxRun::finished);
        QElapsedTimer clock;
        clock.start();
        run.start({u"--"_s, u"sleep"_s, u"30"_s});
        QTRY_COMPARE(finished.size(), 1);
        QVERIFY(clock.elapsed() < 3000);
        QCOMPARE(finished.at(0).at(0).value<SandboxRun::End>(), SandboxRun::End::TimedOut);
        QCOMPARE(finished.at(0).at(1).toInt(), -1);
        QVERIFY(!run.isRunning());
    }

    void cancelIsQuietAndQuick()
    {
        SandboxRun run(5000, this);
        QSignalSpy finished(&run, &SandboxRun::finished);
        run.start({u"--"_s, u"sleep"_s, u"30"_s});
        QTest::qWait(200); // running for real
        QElapsedTimer clock;
        clock.start();
        run.cancel();
        QVERIFY2(clock.elapsed() < 100, "cancel() waited for the process");
        QVERIFY(!run.isRunning());
        QTest::qWait(500);
        QCOMPARE(finished.size(), 0);
    }

    void startCancelsTheRunBefore()
    {
        SandboxRun run(5000, this);
        QSignalSpy finished(&run, &SandboxRun::finished);
        run.start({u"--"_s, u"sleep"_s, u"30"_s});
        run.start({u"--"_s, u"echo"_s, u"second"_s});
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.at(0).at(2).toByteArray(), "second\n");
        QTest::qWait(300);
        QCOMPARE(finished.size(), 1);
    }
};

QTEST_GUILESS_MAIN(TstSandboxRun)
#include "tst_sandboxrun.moc"
