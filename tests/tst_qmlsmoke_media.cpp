// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>

// Smoke tests of the preview window, offscreen: video and audio: covers, playing, full screen.

#include "qmlsmoke.h"

class TstQmlSmokeMedia : public QObject
{
    Q_OBJECT

    QTemporaryDir m_keptDir; // files the shared viewer may still show after a test

private Q_SLOTS:
    void initTestCase()
    {
        const QString error = smokeSetUp();
        QVERIFY2(error.isEmpty(), qPrintable(error));
    }

    void init()
    {
        smokeInit();
    }

    void showsAnAudioCover()
    {
        // The cover comes from KDE's audio thumbnailer (kio-extras), picked from the
        // thumbnailers Dolphin uses; KIO's own default list has none for audio.
        const QString ffmpeg = QStandardPaths::findExecutable(u"ffmpeg"_s);
        if (ffmpeg.isEmpty() || !KPluginMetaData::findPluginById(u"kf6/thumbcreator"_s, u"audiothumbnail"_s).isValid()) {
            QSKIP("needs ffmpeg and KDE's audio thumbnailer");
        }
        QImage picture(64, 64, QImage::Format_RGB32);
        picture.fill(Qt::red);
        const QString png = m_keptDir.filePath(u"cover.png"_s);
        QVERIFY(picture.save(png));
        const QString flac = m_keptDir.filePath(u"with-cover.flac"_s);
        QProcess make;
        make.start(ffmpeg, {u"-v"_s, u"error"_s, u"-y"_s, u"-f"_s, u"lavfi"_s, u"-i"_s, u"anullsrc=r=8000:cl=mono"_s, u"-i"_s, png, u"-t"_s, u"1"_s,
                            u"-map"_s, u"0:a"_s, u"-map"_s, u"1:v"_s, u"-c:v"_s, u"copy"_s, u"-disposition:v"_s, u"attached_pic"_s,
                            u"-metadata:s:v"_s, u"comment=Cover (front)"_s, flac});
        make.closeWriteChannel();
        QVERIFY(make.waitForFinished(20000));
        QCOMPARE(make.exitCode(), 0);

        MainWindow main;
        QQuickWindow *window = main.window();
        QVERIFY(window);
        KwikLookSettings::self()->setAutoplayMedia(false);
        Viewer *v = Viewer::instance();
        v->setWindow(window);
        v->Open({flac}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QQuickItem *cover = nullptr;
        QTRY_VERIFY((cover = window->findChild<QQuickItem *>(u"cover"_s)));
        QTRY_VERIFY_WITH_TIMEOUT(cover->property("hasThumbnail").toBool(), 15000);
        v->Close();
        v->setWindow(nullptr);
    }

    void playsAudio()
    {
        MainWindow main;
        QQuickWindow *window = main.window();
        QVERIFY(window);
        // Two seconds of silence as WAV (16-bit mono, 8 kHz).
        QFile wav(m_keptDir.filePath(u"silence.wav"_s));
        QVERIFY(wav.open(QIODevice::WriteOnly));
        const quint32 rate = 8000;
        const quint32 dataSize = rate * 2 * 2;
        auto le32 = [](quint32 v) { return QByteArray(reinterpret_cast<const char *>(&v), 4); };
        auto le16 = [](quint16 v) { return QByteArray(reinterpret_cast<const char *>(&v), 2); };
        wav.write("RIFF" + le32(36 + dataSize) + "WAVEfmt " + le32(16) + le16(1) + le16(1) + le32(rate) + le32(rate * 2) + le16(2) + le16(16) + "data"
                  + le32(dataSize) + QByteArray(dataSize, '\0'));
        wav.close();
        KwikLookSettings::self()->setAutoplayMedia(true);
        KwikLookSettings::self()->setStartMuted(true); // silent anyway: nothing on the speakers
        Viewer *v = Viewer::instance();
        v->setWindow(window);
        v->Open({wav.fileName()}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QObject *player = nullptr;
        QTRY_VERIFY((player = window->findChild<QObject *>(u"mediaPlayer"_s)));
        QTRY_VERIFY(player->property("duration").toLongLong() > 1900);
        QCOMPARE(player->property("hasVideo").toBool(), false);
        QTRY_COMPARE(player->property("playbackState").toInt(), 1); // PlayingState: started by itself
        QTRY_VERIFY(player->property("position").toLongLong() > 0);
        // The play button pauses.
        QQuickItem *play = window->findChild<QQuickItem *>(u"playButton"_s);
        QVERIFY(play);
        QMetaObject::invokeMethod(play, "clicked");
        QTRY_COMPARE(player->property("playbackState").toInt(), 2); // PausedState
        QMetaObject::invokeMethod(play, "clicked");
        QTRY_COMPARE(player->property("playbackState").toInt(), 1);
        // Repeat: the player loops until the preview closes.
        QQuickItem *repeat = window->findChild<QQuickItem *>(u"repeatButton"_s);
        QVERIFY(repeat);
        QMetaObject::invokeMethod(repeat, "toggle");
        QMetaObject::invokeMethod(repeat, "toggled");
        QCOMPARE(v->property("mediaRepeat").toBool(), true);
        QCOMPARE(player->property("loops").toInt(), -1); // MediaPlayer.Infinite
        // Volume: the slider is perceived loudness, the output gets it cubed.
        auto *audio = player->property("audioOutput").value<QObject *>();
        QVERIFY(audio);
        v->setMediaVolume(0.5);
        QCOMPARE(audio->property("volume").toFloat(), 0.125f);
        // No fullscreen for audio.
        QQuickItem *fullScreen = window->findChild<QQuickItem *>(u"fullScreenButton"_s);
        QVERIFY(fullScreen);
        QVERIFY(!fullScreen->isVisible());
        // Closing the preview stops it, ends repeat and keeps the volume.
        v->Close();
        QTRY_COMPARE(player->property("playbackState").toInt(), 0); // StoppedState
        QCOMPARE(v->property("mediaRepeat").toBool(), false);
        KConfig state(KSharedConfig::openStateConfig()->name(), KConfig::SimpleConfig, QStandardPaths::GenericStateLocation); // read from disk
        QCOMPARE(state.group(u"Viewer"_s).readEntry("MediaVolume", 1.0), 0.5);
        v->setMediaVolume(1);
        v->setWindow(nullptr);
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
    }

    void escLeavesFullScreenFirst()
    {
        MainWindow main;
        QQuickWindow *window = main.window();
        QVERIFY(window);
        QFile file(m_keptDir.filePath(u"fullscreen.txt"_s));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("text\n");
        file.close();
        Viewer *v = Viewer::instance();
        v->setWindow(window);
        v->Open({file.fileName()}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QQuickItem *bar = window->findChild<QQuickItem *>(u"titleBar"_s);
        QVERIFY(bar);
        // (Videos go fullscreen; the window's behaviour doesn't depend on the view.)
        window->showFullScreen();
        QTRY_COMPARE(window->visibility(), QWindow::FullScreen);
        QVERIFY(!bar->isVisible());
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_COMPARE(window->visibility(), QWindow::Windowed);
        QVERIFY(window->isVisible());
        QVERIFY(bar->isVisible());
        // Closed while fullscreen: opens as a normal window.
        window->showFullScreen();
        QTRY_COMPARE(window->visibility(), QWindow::FullScreen);
        v->Close();
        QVERIFY(!window->isVisible());
        v->Open({file.fileName()}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QCOMPARE(window->visibility(), QWindow::Windowed);
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!window->isVisible());
        v->setWindow(nullptr);
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
    }
};

QTEST_MAIN(TstQmlSmokeMedia)
#include "tst_qmlsmoke_media.moc"
