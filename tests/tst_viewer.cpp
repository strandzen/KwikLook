// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
// The viewer's state and signals, without a bus (plain windows for the hide timers).
#include "gui/viewer.h"
#include "kwiklooksettings.h"

#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QSaveFile>
#include <QDir>
#include <QTest>
#include <QWindow>

using namespace Qt::StringLiterals;

class TstViewer : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true); // remembered state goes to a test folder
    }

    void openShowsRequestedFile()
    {
        Viewer v(nullptr);
        QSignalSpy changed(&v, &Viewer::CurrentChanged);
        v.Open({u"/tmp/a.txt"_s, u"file:///tmp/b.txt"_s}, 1, u"s1"_s, QString());
        QVERIFY(v.isOpen());
        QCOMPARE(v.count(), 2);
        QCOMPARE(v.currentIndex(), 1);
        QCOMPARE(v.fileName(), u"b.txt"_s);
        QCOMPARE(changed.size(), 1);
        QCOMPARE(changed.at(0).at(0).toString(), u"s1"_s);
        QCOMPARE(changed.at(0).at(1).toString(), u"file:///tmp/b.txt"_s);
        QCOMPARE(changed.at(0).at(2).toInt(), 1);
    }

    void plainPathsBecomeFileUrls()
    {
        Viewer v(nullptr);
        v.Open({u"/tmp/a b.txt"_s}, 0, QString(), QString());
        QCOMPARE(v.currentUrl(), QUrl::fromLocalFile(u"/tmp/a b.txt"_s));
    }

    void indexOutOfRangeIsClamped()
    {
        Viewer v(nullptr);
        v.Open({u"/tmp/a"_s, u"/tmp/b"_s}, 7, QString(), QString());
        QCOMPARE(v.currentIndex(), 1);
        v.Open({u"/tmp/a"_s, u"/tmp/b"_s}, -3, QString(), QString());
        QCOMPARE(v.currentIndex(), 0);
    }

    void emptyOpenIsIgnored()
    {
        Viewer v(nullptr);
        QSignalSpy changed(&v, &Viewer::CurrentChanged);
        v.Open({}, 0, u"s"_s, QString());
        QVERIFY(!v.isOpen());
        QCOMPARE(changed.size(), 0);
    }

    void nextAndPreviousStopAtTheEnds()
    {
        Viewer v(nullptr);
        v.Open({u"/tmp/a"_s, u"/tmp/b"_s}, 0, QString(), QString());
        v.Previous();
        QCOMPARE(v.currentIndex(), 0);
        v.Next();
        QCOMPARE(v.currentIndex(), 1);
        v.Next();
        QCOMPARE(v.currentIndex(), 1);
    }

    void closeReportsTheSessionOnce()
    {
        Viewer v(nullptr);
        QSignalSpy closed(&v, &Viewer::Closed);
        v.Close(); // nothing open
        QCOMPARE(closed.size(), 0);
        v.Open({u"/tmp/a"_s}, 0, u"s1"_s, QString());
        v.Close();
        v.Close();
        QCOMPARE(closed.size(), 1);
        QCOMPARE(closed.at(0).at(0).toString(), u"s1"_s);
        QVERIFY(!v.isOpen());
    }

    void textZoomLastsUntilClosed()
    {
        Viewer v(nullptr);
        v.Open({u"/tmp/a.txt"_s, u"/tmp/b.txt"_s}, 0, QString(), QString());
        v.setProperty("textZoom", 1.5);
        v.Next();
        QCOMPARE(v.property("textZoom").toReal(), 1.5);
        v.Close();
        QCOMPARE(v.property("textZoom").toReal(), 1.0);
    }

    void newSessionClosesThePreviousOne()
    {
        Viewer v(nullptr);
        QSignalSpy closed(&v, &Viewer::Closed);
        v.Open({u"/tmp/a"_s}, 0, u"window-1"_s, QString());
        v.Open({u"/tmp/b"_s}, 0, u"window-1"_s, QString()); // same caller: no Closed
        QCOMPARE(closed.size(), 0);
        v.Open({u"/tmp/c"_s}, 0, u"window-2"_s, QString());
        QCOMPARE(closed.size(), 1);
        QCOMPARE(closed.at(0).at(0).toString(), u"window-1"_s);
    }

    // The shown file going away while the preview is open: currentGone.
    static void writeFile(const QString &path, const QByteArray &data = "text\n")
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data);
    }
    static void openAndRead(Viewer &v, const QStringList &paths)
    {
        v.Open(paths, 0, u"s"_s, QString());
        QTRY_VERIFY(!v.mimeType().isEmpty()); // details read: watched from here
    }

    void deletedFileIsGone()
    {
        QTemporaryDir dir;
        const QString a = dir.filePath(u"a.txt"_s);
        const QString b = dir.filePath(u"b.txt"_s);
        writeFile(a);
        writeFile(b);
        Viewer v(nullptr);
        openAndRead(v, {a, b});
        QVERIFY(!v.currentGone());
        QVERIFY(QFile::remove(a));
        QTRY_VERIFY(v.currentGone());
        // The next file starts afresh.
        v.Next();
        QTRY_COMPARE(v.currentUrl(), QUrl::fromLocalFile(b));
        QVERIFY(!v.currentGone());
    }

    void renamedFileIsGone()
    {
        QTemporaryDir dir;
        const QString a = dir.filePath(u"a.txt"_s);
        writeFile(a);
        Viewer v(nullptr);
        openAndRead(v, {a});
        QVERIFY(QFile::rename(a, dir.filePath(u"renamed.txt"_s)));
        QTRY_VERIFY(v.currentGone());
    }

    void savedFileIsNotGone()
    {
        QTemporaryDir dir;
        const QString a = dir.filePath(u"a.txt"_s);
        writeFile(a);
        Viewer v(nullptr);
        QSignalSpy gone(&v, &Viewer::currentGoneChanged);
        openAndRead(v, {a});
        // An editor that deletes and writes again ...
        QVERIFY(QFile::remove(a));
        writeFile(a, "saved\n");
        // ... and one that writes a copy and renames it over the file.
        QSaveFile save(a);
        QVERIFY(save.open(QIODevice::WriteOnly));
        save.write("saved again\n");
        QVERIFY(save.commit());
        QTest::qWait(800);
        QVERIFY(!v.currentGone());
        QCOMPARE(gone.size(), 0);
    }

    void deletedFolderIsGone()
    {
        QTemporaryDir dir;
        const QString folder = dir.filePath(u"folder"_s);
        QVERIFY(QDir().mkdir(folder));
        Viewer v(nullptr);
        openAndRead(v, {folder});
        QVERIFY(QDir(folder).removeRecursively());
        QTRY_VERIFY(v.currentGone());
    }

    void closedPreviewWatchesNothing()
    {
        QTemporaryDir dir;
        const QString a = dir.filePath(u"a.txt"_s);
        writeFile(a);
        Viewer v(nullptr);
        QSignalSpy gone(&v, &Viewer::currentGoneChanged);
        openAndRead(v, {a});
        v.Close();
        QVERIFY(QFile::remove(a));
        QTest::qWait(800);
        QVERIFY(!v.currentGone());
        QCOMPARE(gone.size(), 0);
    }

    void ignoredOpenReportsClosed()
    {
        // Nothing usable to show: the caller must not think a preview is open.
        Viewer v(nullptr);
        QSignalSpy closed(&v, &Viewer::Closed);
        v.Open({QString()}, 0, u"s1"_s, QString());
        QCOMPARE(closed.size(), 1);
        QCOMPARE(closed.at(0).at(0).toString(), u"s1"_s);
        QVERIFY(!v.isOpen());
        // The same caller's preview already showing stays, and stays reported open.
        v.Open({u"/tmp/a"_s}, 0, u"s2"_s, QString());
        v.Open({QString()}, 0, u"s2"_s, QString());
        QCOMPARE(closed.size(), 1);
        QVERIFY(v.isOpen());
    }

    void openingTheSameIndexAgainStillSignals()
    {
        // A caller re-opening the same file must learn what is current.
        Viewer v(nullptr);
        QSignalSpy changed(&v, &Viewer::CurrentChanged);
        v.Open({u"/tmp/a"_s}, 0, u"s1"_s, QString());
        v.Open({u"/tmp/a"_s}, 0, u"s2"_s, QString());
        QCOMPARE(changed.size(), 2);
        QCOMPARE(changed.at(1).at(0).toString(), u"s2"_s);
    }

    void fileDetailsArriveForTheCurrentFile()
    {
        QTemporaryDir dir;
        const QString txt = dir.filePath(u"a.txt"_s);
        const QString html = dir.filePath(u"b.html"_s);
        for (const QString &path : {txt, html}) {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("hello");
        }
        Viewer v(nullptr);
        v.Open({txt, html}, 0, QString(), QString());
        QCOMPARE(v.mimeType(), QString()); // not read yet
        v.Next(); // before a.txt's details arrive: those must be dropped
        QTRY_COMPARE(v.mimeType(), u"text/html"_s);
        QCOMPARE(v.view(), u"TextView"_s);
        QVERIFY(v.details().contains(u"5 B"_s));
        QTest::qWait(50);
        QCOMPARE(v.mimeType(), u"text/html"_s);
    }

    void infoPanelSections()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(u"pic.png"_s);
        QImage img(30, 20, QImage::Format_RGB32);
        img.fill(Qt::blue);
        QVERIFY(img.save(path));
        Viewer v(nullptr);
        v.Open({path}, 0, QString(), QString());
        QTRY_COMPARE(v.view(), u"ImageView"_s);
        const QVariantList sections = v.infoSections();
        QVERIFY(sections.size() >= 2);
        const QVariantMap file = sections.at(0).toMap();
        const QVariantList rows = file.value(u"rows"_s).toList();
        QCOMPARE(rows.at(0).toMap().value(u"value"_s).toString(), u"pic.png"_s);
        QCOMPARE(sections.at(1).toMap().value(u"rows"_s).toList().at(0).toMap().value(u"value"_s).toString(), u"30 × 20 pixels"_s);
        QCOMPARE(sections.last().toMap().value(u"rows"_s).toList().size(), 5); // owner, group, 3× access
    }

    void infoPanelStateIsRemembered()
    {
        {
            Viewer v(nullptr);
            v.setInfoOpen(false);
            v.setInfoOpen(true);
        }
        Viewer again(nullptr);
        QVERIFY(again.infoOpen());
        again.setInfoOpen(false);
    }

    void missingFileSaysSo()
    {
        Viewer v(nullptr);
        v.Open({u"/nonexistent/kwiklook-test.txt"_s}, 0, QString(), QString());
        QTRY_VERIFY(!v.details().isEmpty());
        QCOMPARE(v.view(), u"FallbackView"_s);
    }

    void restartsAfterBeingHiddenLong()
    {
        Viewer v(nullptr);
        v.setIdleRestartDelay(100);
        QWindow window;
        v.setWindow(&window);
        QSignalSpy restart(&v, &Viewer::restartRequested);
        window.show();
        window.hide();
        QTRY_COMPARE(restart.size(), 1);
        QTest::qWait(300);
        QCOMPARE(restart.size(), 1); // once
        v.setWindow(nullptr);
    }

    void noRestartWhileShownAgain()
    {
        Viewer v(nullptr);
        v.setIdleRestartDelay(200);
        QWindow window;
        v.setWindow(&window);
        QSignalSpy restart(&v, &Viewer::restartRequested);
        window.show();
        window.hide();
        QTest::qWait(100);
        window.show(); // a preview before the time is up
        QTest::qWait(400);
        QCOMPARE(restart.size(), 0);
        window.hide();
        v.setWindow(nullptr);
    }

    void noRestartWhileSettingsAreOpen()
    {
        Viewer v(nullptr);
        v.setIdleRestartDelay(100);
        QWindow window;
        QWindow settings; // stands in for the settings window
        v.setWindow(&window);
        QSignalSpy restart(&v, &Viewer::restartRequested);
        settings.show();
        window.show();
        window.hide();
        QTest::qWait(400);
        QCOMPARE(restart.size(), 0);
        settings.hide();
        QTRY_COMPARE(restart.size(), 1); // checked again later
        v.setWindow(nullptr);
    }

    void noRestartWhenTurnedOff()
    {
        KwikLookSettings::self()->setFreeMemoryWhenIdle(false); // in memory only, not saved
        Viewer v(nullptr);
        v.setIdleRestartDelay(100);
        QWindow window;
        v.setWindow(&window);
        QSignalSpy restart(&v, &Viewer::restartRequested);
        window.show();
        window.hide();
        QTest::qWait(400);
        QCOMPARE(restart.size(), 0);
        v.setWindow(nullptr);
        KwikLookSettings::self()->setFreeMemoryWhenIdle(true);
    }
};

QTEST_MAIN(TstViewer)
#include "tst_viewer.moc"
