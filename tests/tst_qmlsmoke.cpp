// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "gui/autostart.h"
#include "gui/thumbnailprovider.h"
#include "gui/viewer.h"
#include "kwiklooksettings.h"

#include <KLocalizedQmlContext>

#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QDir>
#include <QFontMetricsF>
#include <QWheelEvent>
#include <QPainter>
#include <QPdfWriter>
#include <QQuickItem>
#include <QQuickWindow>
#include <QImage>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QStandardPaths>
#include <QTest>

#include <KConfig>
#include <KConfigGroup>

#include <algorithm>
#include <memory>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(kwiklookPlugin)

using namespace Qt::StringLiterals;

// A 4×4 GIF with two frames (red, blue), made with ImageMagick: Qt can't write animated GIFs.
static const QByteArray TwoFrameGif = QByteArray::fromRawData("\x47\x49\x46\x38\x39\x61\x04\x00\x04\x00\xf0\x00\x00\xff\x00\x00\x00\x00\x00\x21\xff\x0b\x4e\x45\x54\x53\x43\x41\x50\x45\x32\x2e\x30\x03\x01\x00\x00\x00\x21\xf9\x04\x00\x00\x00\x00\x00\x2c\x00\x00\x00\x00\x04\x00\x04\x00\x00\x02\x04\x84\x8f\x09\x05\x00\x21\xf9\x04\x00\x00\x00\x00\x00\x2c\x00\x00\x00\x00\x04\x00\x04\x00\x80\x00\x00\xff\x00\x00\x00\x02\x04\x84\x8f\x09\x05\x00\x3b", 95);

// Qt PDF renders pages on the engine's image thread and crashes if the engine (and
// the PDF document) is deleted mid-render (SIGSEGV in the qpdf image plugin). Tests
// with PDFs wait for their pages before the engine goes.
static bool pdfPagesRendering(QQuickItem *item)
{
    if (item->inherits("QQuickPdfPageImage") && item->property("status").toInt() == 2 /* Image.Loading */) {
        return true;
    }
    const auto children = item->childItems();
    return std::any_of(children.begin(), children.end(), pdfPagesRendering);
}

class TstQmlSmoke : public QObject
{
    Q_OBJECT

    QTemporaryDir m_keptDir; // files the shared viewer may still show after a test

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true); // remembered state goes to a test folder
    }

    void init()
    {
        // The viewer is shared by all tests: a window closed at the end of one test
        // leaves the app without focus, and “close when you switch” would then close
        // the next test's preview 150 ms later. Not under test here.
        KwikLookSettings::self()->setCloseOnFocusLoss(false);
    }

    void loadsMainWindow()
    {
        QQmlApplicationEngine engine;
        KLocalization::setupLocalizedContext(&engine);
        QList<QQmlError> errors;
        connect(&engine, &QQmlApplicationEngine::warnings, this, [&errors](const QList<QQmlError> &w) {
            errors << w;
        });
        engine.loadFromModule("kwiklook", "Main");
        QCOMPARE(engine.rootObjects().size(), 1);
        for (const QQmlError &e : std::as_const(errors)) {
            qWarning().noquote() << e.toString();
        }
        QVERIFY(errors.isEmpty());
    }

    void loadsFallbackViewForAFile()
    {
        QQmlApplicationEngine engine;
        KLocalization::setupLocalizedContext(&engine);
        engine.addImageProvider(QLatin1String(ThumbnailProvider::Name), new ThumbnailProvider);
        QList<QQmlError> errors;
        connect(&engine, &QQmlApplicationEngine::warnings, this, [&errors](const QList<QQmlError> &w) {
            errors << w;
        });
        engine.loadFromModule("kwiklook", "Main");
        QCOMPARE(engine.rootObjects().size(), 1);
        QTemporaryFile file(QDir::tempPath() + QStringLiteral("/kwiklook-XXXXXX.bin"));
        QVERIFY(file.open());
        file.write(QByteArray(3000, '\x07')); // binary: no view of its own
        file.flush();
        Viewer::instance()->Open({file.fileName()}, 0, QString(), QString());
        QTRY_COMPARE(Viewer::instance()->view(), QStringLiteral("FallbackView"));
        QTRY_VERIFY(engine.rootObjects().constFirst()->findChild<QQuickItem *>(QStringLiteral("fallbackView")));
        QTest::qWait(200); // let the thumbnail request finish or fail
        for (const QQmlError &e : std::as_const(errors)) {
            qWarning().noquote() << e.toString();
        }
        QVERIFY(errors.isEmpty());
    }

    void loadsImageViewWithInfoPanel()
    {
        QQmlApplicationEngine engine;
        KLocalization::setupLocalizedContext(&engine);
        engine.addImageProvider(QLatin1String(ThumbnailProvider::Name), new ThumbnailProvider);
        QList<QQmlError> errors;
        connect(&engine, &QQmlApplicationEngine::warnings, this, [&errors](const QList<QQmlError> &w) {
            errors << w;
        });
        engine.loadFromModule("kwiklook", "Main");
        QCOMPARE(engine.rootObjects().size(), 1);
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("pic.png"));
        QImage img(40, 30, QImage::Format_RGB32);
        img.fill(Qt::green);
        QVERIFY(img.save(path));
        Viewer::instance()->setInfoOpen(true);
        Viewer::instance()->Open({path}, 0, QString(), QString());
        QObject *root = engine.rootObjects().constFirst();
        QTRY_VERIFY(root->findChild<QQuickItem *>(QStringLiteral("imageView")));
        QTRY_VERIFY(root->findChild<QQuickItem *>(QStringLiteral("infoPanel")));
        QTest::qWait(300);
        Viewer::instance()->setInfoOpen(false);
        for (const QQmlError &e : std::as_const(errors)) {
            qWarning().noquote() << e.toString();
        }
        QVERIFY(errors.isEmpty());
    }


    void playsAnimatedPictures()
    {
        QQmlApplicationEngine engine;
        KLocalization::setupLocalizedContext(&engine);
        engine.addImageProvider(QLatin1String(ThumbnailProvider::Name), new ThumbnailProvider);
        QList<QQmlError> errors;
        connect(&engine, &QQmlApplicationEngine::warnings, this, [&errors](const QList<QQmlError> &w) {
            errors << w;
        });
        engine.loadFromModule("kwiklook", "Main");
        QObject *root = engine.rootObjects().constFirst();
        // Kept until the end: the viewer still points at it when the next test starts.
        QFile gif(m_keptDir.filePath(u"moving.gif"_s));
        QVERIFY(gif.open(QIODevice::WriteOnly));
        gif.write(TwoFrameGif);
        gif.close();
        Viewer::instance()->Open({gif.fileName()}, 0, QString(), QString());
        QQuickItem *animated = nullptr;
        QTRY_VERIFY((animated = root->findChild<QQuickItem *>(u"animatedImage"_s)));
        QTRY_COMPARE(animated->property("frameCount").toInt(), 2);
        QVERIFY(animated->property("playing").toBool());
        QVERIFY(animated->width() > 0);
        for (const QQmlError &e : std::as_const(errors)) {
            qWarning().noquote() << e.toString();
        }
        QVERIFY(errors.isEmpty());
    }

    void loadsTextViewAndKeysStillNavigate()
    {
        QQmlApplicationEngine engine;
        KLocalization::setupLocalizedContext(&engine);
        engine.addImageProvider(QLatin1String(ThumbnailProvider::Name), new ThumbnailProvider);
        QList<QQmlError> errors;
        connect(&engine, &QQmlApplicationEngine::warnings, this, [&errors](const QList<QQmlError> &w) {
            errors << w;
        });
        engine.loadFromModule("kwiklook", "Main");
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QTemporaryDir dir;
        QStringList files;
        for (const QString &name : {QStringLiteral("a.cpp"), QStringLiteral("b.md")}) {
            QFile f(dir.filePath(name));
            QVERIFY(f.open(QIODevice::WriteOnly));
            for (int i = 0; i < 200; ++i) {
                f.write("int main() { return 0; } // a line of code\n");
            }
            files << f.fileName();
        }
        Viewer *v = Viewer::instance();
        v->setWindow(window);
        v->Open(files, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTRY_VERIFY(window->findChild<QQuickItem *>(QStringLiteral("textView")));
        // The text field has the focus (as after a click into it): ← → and Space
        // still belong to the preview.
        auto *area = window->findChild<QQuickItem *>(QStringLiteral("textArea"));
        QVERIFY(area);
        area->forceActiveFocus();
        QTRY_VERIFY(area->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_Right);
        QTRY_COMPARE(v->currentIndex(), 1);
        QTest::keyClick(window, Qt::Key_Left);
        QTRY_COMPARE(v->currentIndex(), 0);
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(!window->isVisible());
        v->setWindow(nullptr);
        for (const QQmlError &e : std::as_const(errors)) {
            qWarning().noquote() << e.toString();
        }
        QVERIFY(errors.isEmpty());
    }

    void textViewColoursScriptsAndKeepsLineNumbers()
    {
        QQmlApplicationEngine engine;
        KLocalization::setupLocalizedContext(&engine);
        engine.addImageProvider(QLatin1String(ThumbnailProvider::Name), new ThumbnailProvider);
        engine.loadFromModule("kwiklook", "Main");
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QTemporaryDir dir;
        // A shell script without an extension: coloured by its type, found from the content.
        QFile f(dir.filePath(u"run-me"_s));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("#!/bin/sh\n");
        for (int i = 0; i < 300; ++i) {
            f.write("echo \"line\"\n");
        }
        f.close();
        Viewer *v = Viewer::instance();
        v->setWindow(window);
        v->Open({f.fileName()}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QQuickItem *area = nullptr;
        QTRY_VERIFY((area = window->findChild<QQuickItem *>(u"textArea"_s)));
        QObject *highlighter = area->findChild<QObject *>(u"highlighter"_s);
        QVERIFY(highlighter);
        const QVariant definition = highlighter->property("definition");
        const QMetaObject *meta = definition.metaType().metaObject();
        QVERIFY(meta);
        QTRY_COMPARE(meta->property(meta->indexOfProperty("name")).readOnGadget(highlighter->property("definition").constData()).toString(), u"Bash"_s);

        // Scrolling moves the numbers, it doesn't make new labels.
        const auto labels = [area] {
            QList<QQuickItem *> found;
            for (QQuickItem *item : area->childItems()) {
                if (item->objectName() == u"lineNumber"_s) {
                    found << item;
                }
            }
            return found;
        };
        const auto shown = [&labels] {
            QStringList numbers;
            for (QQuickItem *label : labels()) {
                if (label->isVisible()) {
                    numbers << label->property("text").toString();
                }
            }
            return numbers;
        };
        QTRY_VERIFY(shown().contains(u"1"_s));
        const QList<QQuickItem *> before = labels();
        QVERIFY(!before.isEmpty());
        QObject *flickable = area->parentItem()->parentItem(); // ScrollView's Flickable
        QVERIFY(flickable->setProperty("contentY", 100 * QFontMetricsF(area->property("font").value<QFont>()).lineSpacing()));
        QTRY_VERIFY2(!shown().contains(u"1"_s) && shown().contains(u"110"_s),
                     qPrintable(u"shown %1; contentY %2, contentHeight %3, height %4, visible %5"_s.arg(shown().join(u","_s))
                                    .arg(flickable->property("contentY").toReal())
                                    .arg(flickable->property("contentHeight").toReal())
                                    .arg(flickable->property("height").toReal())
                                    .arg(window->isVisible())));
        QCOMPARE(labels(), before);
        v->Close();
        v->setWindow(nullptr);
    }

    void loadsPdfView()
    {
        QQmlApplicationEngine engine;
        KLocalization::setupLocalizedContext(&engine);
        engine.addImageProvider(QLatin1String(ThumbnailProvider::Name), new ThumbnailProvider);
        QList<QQmlError> errors;
        connect(&engine, &QQmlApplicationEngine::warnings, this, [&errors](const QList<QQmlError> &w) {
            errors << w;
        });
        engine.loadFromModule("kwiklook", "Main");
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QTemporaryDir dir;
        const QString pdf = dir.filePath(QStringLiteral("doc.pdf"));
        {
            QPdfWriter writer(pdf);
            QPainter painter(&writer);
            painter.drawText(QPointF(100, 100), QStringLiteral("Page one"));
            writer.newPage();
            painter.drawText(QPointF(100, 100), QStringLiteral("Page two"));
        }
        QFile other(dir.filePath(QStringLiteral("other.bin")));
        QVERIFY(other.open(QIODevice::WriteOnly));
        other.write(QByteArray(100, '\x01'));
        other.close();
        Viewer *v = Viewer::instance();
        v->setWindow(window);
        v->Open({pdf, other.fileName()}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTRY_VERIFY(window->findChild<QQuickItem *>(QStringLiteral("pdfView")));
        QTest::qWait(500); // pages render
        QTest::keyClick(window, Qt::Key_Right);
        QTRY_COMPARE(v->currentIndex(), 1);
        QTRY_VERIFY(!pdfPagesRendering(window->contentItem()));
        v->Close();
        v->setWindow(nullptr);
        for (const QQmlError &e : std::as_const(errors)) {
            qWarning().noquote() << e.toString();
        }
        QVERIFY(errors.isEmpty());
    }

    void zoomsPicturesTextAndPdf()
    {
        QQmlApplicationEngine engine;
        KLocalization::setupLocalizedContext(&engine);
        engine.addImageProvider(QLatin1String(ThumbnailProvider::Name), new ThumbnailProvider);
        QList<QQmlError> errors;
        connect(&engine, &QQmlApplicationEngine::warnings, this, [&errors](const QList<QQmlError> &w) {
            errors << w;
        });
        engine.loadFromModule("kwiklook", "Main");
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QTemporaryDir dir;
        QStringList files;
        for (const QString &name : {u"big.png"_s, u"big2.png"_s}) {
            QImage img(3000, 2000, QImage::Format_RGB32);
            img.fill(Qt::darkCyan);
            QVERIFY(img.save(dir.filePath(name)));
            files << dir.filePath(name);
        }
        QFile text(dir.filePath(u"a.txt"_s));
        QVERIFY(text.open(QIODevice::WriteOnly));
        text.write(QByteArray("some text\n").repeated(50));
        text.close();
        files << text.fileName() << text.fileName();
        const QString pdf = dir.filePath(u"doc.pdf"_s);
        {
            QPdfWriter writer(pdf);
            QPainter painter(&writer);
            painter.drawText(QPointF(100, 100), u"Page one"_s);
        }
        files << pdf;

        const auto ctrlWheel = [window](QQuickItem *over, int steps) {
            const QPointF pos = over->mapToScene(QPointF(over->width() / 2, over->height() / 2));
            QWheelEvent event(pos, window->mapToGlobal(pos), QPoint(), QPoint(0, 120 * steps), Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
            QCoreApplication::sendEvent(window, &event);
        };
        QObject *indicator = window->findChild<QObject *>(u"zoomIndicator"_s);
        QVERIFY(indicator);

        Viewer *v = Viewer::instance();
        v->setWindow(window);
        v->Open(files, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));

        // Picture: fitted first; Ctrl+wheel zooms in, the indicator shows, Ctrl+0 fits again.
        QQuickItem *image = nullptr;
        QTRY_VERIFY((image = window->findChild<QQuickItem *>(u"imageView"_s)));
        QTRY_VERIFY(image->property("fit").toReal() > 0);
        const qreal fit = image->property("fit").toReal();
        QVERIFY(fit < 1);
        ctrlWheel(image, 1);
        QCOMPARE(image->property("zoom").toReal(), fit * 1.25);
        QTRY_COMPARE(indicator->property("opacity").toReal(), 1.0);
        QTest::keyClick(window, Qt::Key_Plus, Qt::ControlModifier);
        QCOMPARE(image->property("zoom").toReal(), fit * 1.25 * 1.25);
        QTest::keyClick(window, Qt::Key_0, Qt::ControlModifier);
        QCOMPARE(image->property("zoom").toReal(), fit);
        ctrlWheel(image, -1); // not below fitted
        QCOMPARE(image->property("zoom").toReal(), fit);
        for (int i = 0; i < 20; ++i) { // not past 800 %, and through real size
            ctrlWheel(image, 1);
        }
        QCOMPARE(image->property("zoom").toReal(), 8.0);
        QVERIFY(image->property("pannable").toBool());
        // The next picture starts fitted.
        v->Next();
        QTRY_COMPARE(image->property("userZoom").toReal(), 0.0);
        QTRY_COMPARE(image->property("zoom").toReal(), fit); // once its size is read

        // Text: the font grows and stays for the next file.
        v->Next();
        QQuickItem *area = nullptr;
        QTRY_VERIFY((area = window->findChild<QQuickItem *>(u"textArea"_s)));
        const qreal pointSize = area->property("font").value<QFont>().pointSizeF();
        ctrlWheel(window->findChild<QQuickItem *>(u"textView"_s), 1); // the area is as tall as the text
        QCOMPARE(v->property("textZoom").toReal(), 1.1);
        QCOMPARE(area->property("font").value<QFont>().pointSizeF(), pointSize * 1.1);
        v->Next();
        QCOMPARE(v->property("textZoom").toReal(), 1.1);

        // PDF: wider than the view when zoomed in.
        v->Next();
        QQuickItem *pdfView = nullptr;
        QTRY_VERIFY((pdfView = window->findChild<QQuickItem *>(u"pdfView"_s)));
        QTRY_VERIFY(pdfView->property("zoomPercent").toInt() > 0);
        QTest::qWait(300);
        ctrlWheel(pdfView, 2);
        QCOMPARE(pdfView->property("zoom").toReal(), 1.25 * 1.25);
        QTest::keyClick(window, Qt::Key_Minus, Qt::ControlModifier);
        QCOMPARE(pdfView->property("zoom").toReal(), 1.25);

        QTRY_VERIFY(!pdfPagesRendering(window->contentItem()));
        v->Close();
        QCOMPARE(v->property("textZoom").toReal(), 1.0);
        v->setWindow(nullptr);
        for (const QQmlError &e : std::as_const(errors)) {
            qWarning().noquote() << e.toString();
        }
        QVERIFY(errors.isEmpty());
    }

    void pdfPagesStayCentredAndInside()
    {
        QQmlApplicationEngine engine;
        KLocalization::setupLocalizedContext(&engine);
        engine.addImageProvider(QLatin1String(ThumbnailProvider::Name), new ThumbnailProvider);
        engine.loadFromModule("kwiklook", "Main");
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QTemporaryDir dir;
        const QString pdf = dir.filePath(u"pages.pdf"_s);
        {
            QPdfWriter writer(pdf);
            QPainter painter(&writer);
            for (int i = 0; i < 6; ++i) {
                if (i > 0) {
                    writer.newPage();
                }
                painter.drawText(QPointF(100, 100), u"Page %1"_s.arg(i + 1));
            }
        }
        Viewer *v = Viewer::instance();
        v->setWindow(window);
        v->Open({pdf}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QQuickItem *pdfView = nullptr;
        QTRY_VERIFY((pdfView = window->findChild<QQuickItem *>(u"pdfView"_s)));
        QTRY_VERIFY(pdfView->property("zoomPercent").toInt() > 0);

        QQuickItem *list = nullptr;
        for (QQuickItem *item : pdfView->findChildren<QQuickItem *>()) {
            if (item->inherits("QQuickListView")) {
                list = item;
            }
        }
        QVERIFY(list);
        auto *content = list->property("contentItem").value<QQuickItem *>();
        // Every page's centre is the centre of the list's content, whatever was scrolled.
        const auto offCentre = [&] {
            const qreal centre = list->property("contentWidth").toReal() / 2;
            QStringList wrong;
            int pages = 0;
            QList<QQuickItem *> papers;
            for (QQuickItem *row : content->childItems()) {
                for (QQuickItem *child : row->childItems()) {
                    if (child->objectName() == u"pdfPage"_s && row->isVisible()) {
                        papers << child;
                    }
                }
            }
            for (QQuickItem *paper : std::as_const(papers)) {
                ++pages;
                const qreal x = paper->mapToItem(content, QPointF(paper->width() / 2, 0)).x();
                if (qAbs(x - centre) > 1) {
                    wrong << u"page centre %1, content centre %2"_s.arg(x).arg(centre);
                }
            }
            return pages > 0 ? wrong.join(u"; "_s) : u"no pages"_s;
        };
        QTRY_COMPARE(offCentre(), QString());
        for (int i = 0; i < 20; ++i) { // scroll down, pages get created and reused
            QMetaObject::invokeMethod(list, "flick", Q_ARG(qreal, 0), Q_ARG(qreal, -3000));
            QTest::qWait(50);
        }
        QTest::qWait(500);
        QCOMPARE(offCentre(), QString()); // scrolled
        QTest::keyClick(window, Qt::Key_Plus, Qt::ControlModifier);
        QTest::keyClick(window, Qt::Key_Plus, Qt::ControlModifier);
        QTest::qWait(300);
        QCOMPARE(offCentre(), QString()); // zoomed
        list->setProperty("contentY", 0);
        QTest::qWait(300);
        QCOMPARE(offCentre(), QString()); // zoomed, back at the top
        // Pages scrolled up stay below the bar: the view area clips.
        QQuickItem *area = pdfView->parentItem()->parentItem();
        QVERIFY(area->clip());
        QTRY_VERIFY(!pdfPagesRendering(window->contentItem()));
        v->Close();
        v->setWindow(nullptr);
    }

    void settingsWindowAppliesOnlyOnApply()
    {
        // Test mode: kwiklookrc and the autostart entry go to ~/.qttest, not ~/.config.
        QVERIFY(Autostart::filePath().contains(u".qttest"_s));
        QQmlApplicationEngine engine;
        KLocalization::setupLocalizedContext(&engine);
        QList<QQmlError> errors;
        connect(&engine, &QQmlApplicationEngine::warnings, this, [&errors](const QList<QQmlError> &w) {
            errors << w;
        });
        QQmlComponent component(&engine, "kwiklook", "SettingsWindow");
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(object.get());
        QVERIFY(window);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window));

        KwikLookSettings *settings = KwikLookSettings::self();
        QCOMPARE(window->property("openKey").toString(), settings->openKey());
        QCOMPARE(window->property("changed").toBool(), false);

        // A change is only shown until Apply.
        window->setProperty("openKey", u"Shift+Space"_s);
        window->setProperty("matchBreeze", false);
        window->setProperty("radius", 12);
        window->setProperty("startAtLogin", true);
        QCOMPARE(window->property("changed").toBool(), true);
        QCOMPARE(settings->openKey(), u"Space"_s);
        QVERIFY(!QFile::exists(Autostart::filePath()));

        QVariant ok;
        QVERIFY(QMetaObject::invokeMethod(window, "apply", Q_RETURN_ARG(QVariant, ok)));
        QVERIFY(ok.toBool());
        QCOMPARE(settings->openKey(), u"Shift+Space"_s);
        QCOMPARE(settings->matchBreezeCorners(), false);
        QCOMPARE(settings->cornerRadius(), 12);
        QCOMPARE(window->property("changed").toBool(), false);
        KConfig saved(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u"/kwiklookrc"_s, KConfig::SimpleConfig);
        QCOMPARE(saved.group(u"General"_s).readEntry("OpenKey"), u"Shift+Space"_s);
        QFile entry(Autostart::filePath());
        QVERIFY(entry.open(QIODevice::ReadOnly));
        QVERIFY(entry.readAll().contains("Exec=kwiklook --service\n"));
        entry.close();

        // Cancel-like: closing and reopening shows the saved values again.
        window->setProperty("openKey", u"Ctrl+Space"_s);
        window->close();
        window->show();
        QCOMPARE(window->property("openKey").toString(), u"Shift+Space"_s);

        // Back to the defaults, entry removed.
        window->setProperty("openKey", u"Space"_s);
        window->setProperty("matchBreeze", true);
        window->setProperty("radius", 5);
        window->setProperty("startAtLogin", false);
        QVERIFY(QMetaObject::invokeMethod(window, "apply", Q_RETURN_ARG(QVariant, ok)));
        QVERIFY(!QFile::exists(Autostart::filePath()));
        QCOMPARE(settings->openKey(), u"Space"_s);

        for (const QQmlError &e : std::as_const(errors)) {
            qWarning().noquote() << e.toString();
        }
        QVERIFY(errors.isEmpty());
    }
};

QTEST_MAIN(TstQmlSmoke)
#include "tst_qmlsmoke.moc"
