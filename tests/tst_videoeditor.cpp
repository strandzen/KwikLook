// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "gui/videoeditor.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

class TstVideoEditor : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    QString m_video;

    static QByteArray hash(const QString &path)
    {
        QFile f(path);
        return f.open(QIODevice::ReadOnly) ? QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha1) : QByteArray();
    }
    static QString probe(const QString &path, const QString &entries)
    {
        QProcess p;
        p.start(u"ffprobe"_s, {u"-v"_s, u"error"_s, u"-select_streams"_s, u"v:0"_s, u"-show_entries"_s, entries, u"-of"_s, u"csv=p=0"_s, path});
        p.waitForFinished(10000);
        return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    }
    // The scratch folders the editor uses (in the test cache folder).
    static QStringList workDirs()
    {
        return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + u"/kwiklook/video"_s).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true); // the scratch folder goes to a test cache
        if (!VideoEditor::available()) {
            QSKIP("ffmpeg, ffprobe or the sandbox isn't available");
        }
        // Six seconds, a keyframe every second, with sound.
        m_video = m_dir.filePath(u"clip.mp4"_s);
        QProcess ffmpeg;
        ffmpeg.start(u"ffmpeg"_s, {u"-loglevel"_s, u"error"_s, u"-y"_s, u"-f"_s, u"lavfi"_s, u"-i"_s, u"testsrc=duration=6:size=160x120:rate=25"_s,
                                   u"-f"_s, u"lavfi"_s, u"-i"_s, u"sine=frequency=440:duration=6"_s, u"-shortest"_s, u"-g"_s, u"25"_s,
                                   u"-pix_fmt"_s, u"yuv420p"_s, m_video});
        QVERIFY(ffmpeg.waitForFinished(30000) && ffmpeg.exitCode() == 0);
    }

    void listsKeyframes()
    {
        VideoEditor editor;
        editor.setSource(QUrl::fromLocalFile(m_video));
        QSignalSpy probed(&editor, &VideoEditor::probed);
        editor.probe();
        QVERIFY(editor.probing());
        QTRY_VERIFY(!editor.probing());
        QCOMPARE(editor.keyframes(), (QList<qreal>{0, 1, 2, 3, 4, 5}));
        QCOMPARE(editor.rotation(), 0);
        QCOMPARE(editor.keyframeAtOrBefore(2.7), 2.0);
        QCOMPARE(editor.keyframeAtOrBefore(3.0), 3.0);
    }

    void trimsAndRotatesACopy()
    {
        const QByteArray before = hash(m_video);
        VideoEditor editor;
        editor.setSource(QUrl::fromLocalFile(m_video));
        editor.probe();
        QTRY_VERIFY(!editor.probing());
        QSignalSpy saved(&editor, &VideoEditor::saved);
        QSignalSpy failed(&editor, &VideoEditor::failed);
        editor.save(2, 4.5, 90);
        QVERIFY(editor.saving());
        QTRY_VERIFY_WITH_TIMEOUT(saved.size() + failed.size() == 1, 30000);
        QVERIFY2(failed.isEmpty(), failed.isEmpty() ? "" : qPrintable(failed.first().first().toString()));
        const QString copy = saved.first().first().toUrl().toLocalFile();
        QCOMPARE(copy, m_dir.filePath(u"clip (edited).mp4"_s));
        // About 2.5 s (a lossless cut ends on whole packets), turned by 90°.
        const double duration = probe(copy, u"format=duration"_s).toDouble();
        QVERIFY2(duration > 2.3 && duration < 3.0, qPrintable(QString::number(duration)));
        QCOMPARE(probe(copy, u"stream_side_data=rotation"_s), u"90"_s);
        // The original is untouched; the scratch folder is gone.
        QCOMPARE(hash(m_video), before);
        QVERIFY(workDirs().isEmpty());

        // Again: a new name, the first copy is kept.
        saved.clear();
        editor.save(0, 0, 0);
        QTRY_VERIFY_WITH_TIMEOUT(saved.size() == 1, 30000);
        const QString second = saved.first().first().toUrl().toLocalFile();
        QVERIFY(second != copy);
        QVERIFY(QFile::exists(copy) && QFile::exists(second));
        QCOMPARE(probe(second, u"stream_side_data=rotation"_s), QString()); // no turn: none added
    }

    void cancelLeavesNothing()
    {
        VideoEditor editor;
        editor.setSource(QUrl::fromLocalFile(m_video));
        QSignalSpy saved(&editor, &VideoEditor::saved);
        const qsizetype files = QDir(m_dir.path()).count();
        editor.save(1, 5, 270);
        editor.cancel();
        QVERIFY(!editor.saving());
        QTest::qWait(500);
        QVERIFY(saved.isEmpty());
        QCOMPARE(QDir(m_dir.path()).count(), files);
        QVERIFY(workDirs().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstVideoEditor)
#include "tst_videoeditor.moc"
