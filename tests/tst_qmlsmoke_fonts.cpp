// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>

// Smoke tests of the preview window, offscreen: fonts: the sample, typed text, installing.

#include "qmlsmoke.h"

class TstQmlSmokeFonts : public QObject
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

    void showsFontsAndTypedText()
    {
        QDirIterator it(u"/usr/share/fonts"_s, {u"*.ttf"_s, u"*.otf"_s}, QDir::Files, QDirIterator::Subdirectories);
        if (!it.hasNext()) {
            QSKIP("no TrueType or OpenType font in /usr/share/fonts");
        }
        const QString fontPath = it.next();
        // Not a font, though named like one: the fallback view.
        QFile broken(m_keptDir.filePath(u"broken.ttf"_s));
        QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write(QByteArray(4096, '\x01'));
        broken.close();
        MainWindow main;
        QQuickWindow *window = main.window();
        QVERIFY(window);
        Viewer *v = Viewer::instance();
        v->setWindow(window);
        v->Open({fontPath, broken.fileName()}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTRY_VERIFY(window->findChild<QQuickItem *>(u"fontView"_s));
        QVERIFY(!v->fontPreview()->family().isEmpty());
        QQuickItem *sample = findItem(window, u"fontSample"_s); // a Repeater row
        QVERIFY(sample);
        QCOMPARE(sample->property("font").value<QFont>().family(), v->fontPreview()->family());
        // A font from /usr/share/fonts is installed: no button, and nothing copied.
        QVERIFY(v->fontPreview()->installed());
        QVERIFY(findItem(window, u"fontInstalled"_s)->isVisible());
        QVERIFY(!findItem(window, u"installFont"_s)->isVisible());
        v->fontPreview()->install();
        QVERIFY(!QFileInfo::exists(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + u"/fonts"_s));
        // One size: 36 px, − / + through the usual sizes, Ctrl+0 back.
        QCOMPARE(sample->property("font").value<QFont>().pixelSize(), 36);
        QMetaObject::invokeMethod(findItem(window, u"fontLarger"_s), "clicked");
        QCOMPARE(v->fontPreview()->property("sampleSize").toInt(), 48);
        QCOMPARE(sample->property("font").value<QFont>().pixelSize(), 48);
        QMetaObject::invokeMethod(findItem(window, u"fontSmaller"_s), "clicked");
        QMetaObject::invokeMethod(findItem(window, u"fontSmaller"_s), "clicked");
        QCOMPARE(v->fontPreview()->property("sampleSize").toInt(), 28);
        QTest::keyClick(window, Qt::Key_0, Qt::ControlModifier);
        QCOMPARE(v->fontPreview()->property("sampleSize").toInt(), 36);
        QTest::keyClick(window, Qt::Key_Plus, Qt::ControlModifier);
        QCOMPARE(v->fontPreview()->property("sampleSize").toInt(), 45); // Ctrl+plus: × 1.25
        // Typing: the samples show the text; Enter and ← → don't act on the preview.
        auto *field = window->findChild<QQuickItem *>(u"fontSampleField"_s);
        QVERIFY(field);
        field->forceActiveFocus();
        for (const char c : {'H', 'e', 'l', 'l', 'o'}) {
            QTest::keyClick(window, c); // no keyClicks() for QWindow
        }
        QCOMPARE(v->fontPreview()->property("sampleText").toString(), u"Hello"_s);
        QCOMPARE(sample->property("text").toString(), u"Hello"_s);
        QTest::keyClick(window, Qt::Key_Right);
        QTest::keyClick(window, Qt::Key_Return);
        QCOMPARE(v->currentIndex(), 0);
        QVERIFY(window->isVisible());
        // The text stays for the next file; a broken font shows the fallback.
        v->Next();
        QTRY_VERIFY(window->findChild<QQuickItem *>(u"fallbackView"_s));
        QVERIFY(v->fontPreview()->family().isEmpty());
        QCOMPARE(v->fontPreview()->property("sampleText").toString(), u"Hello"_s);
        QCOMPARE(v->fontPreview()->property("sampleSize").toInt(), 45); // kept too
        // Closing forgets them.
        v->Close();
        QCOMPARE(v->fontPreview()->property("sampleText").toString(), QString());
        QCOMPARE(v->fontPreview()->property("sampleSize").toInt(), 36);
        v->setWindow(nullptr);
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
    }

    void installsAFont()
    {
        // A font fontconfig doesn't know: from an application's own folder (these vary
        // between systems; skipped when none is found or it turns out installed).
        QString fontPath;
        for (const QString &dir : {u"/usr/share/imlib2/data/fonts"_s, u"/usr/share/projectM/fonts"_s, u"/usr/share/tessdata"_s}) {
            QDirIterator it(dir, {u"*.ttf"_s}, QDir::Files);
            while (it.hasNext() && fontPath.isEmpty()) {
                const QString path = it.next();
                const QRawFont raw(path, 12);
                if (raw.isValid() && !QFontDatabase::families().contains(raw.familyName())) {
                    fontPath = path;
                }
            }
        }
        if (fontPath.isEmpty()) {
            QSKIP("no font outside fontconfig's folders found");
        }
        const QString fonts = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + u"/fonts"_s;
        QVERIFY(fonts.contains(u".qttest"_s)); // test mode: not the user's fonts
        QDir(fonts).removeRecursively();
        const auto cleanup = qScopeGuard([fonts] {
            QDir(fonts).removeRecursively();
        });
        MainWindow main;
        QQuickWindow *window = main.window();
        QVERIFY(window);
        Viewer *v = Viewer::instance();
        v->setWindow(window);
        v->Open({fontPath}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTRY_VERIFY(window->findChild<QQuickItem *>(u"fontView"_s));
        QVERIFY(!v->fontPreview()->installed());
        QQuickItem *button = findItem(window, u"installFont"_s);
        QVERIFY(button && button->isVisible());
        QSignalSpy finished(v->fontPreview(), &FontPreview::installFinished);
        QMetaObject::invokeMethod(button, "clicked");
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.first().first().toBool(), true);
        const QString copy = fonts + u'/' + QFileInfo(fontPath).fileName();
        QVERIFY(QFileInfo::exists(copy));
        QVERIFY(v->fontPreview()->installed());
        QTRY_VERIFY(findItem(window, u"fontInstalled"_s)->isVisible());
        v->Close();
        v->setWindow(nullptr);
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
    }
};

QTEST_MAIN(TstQmlSmokeFonts)
#include "tst_qmlsmoke_fonts.moc"
