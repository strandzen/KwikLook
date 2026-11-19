// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "gui/autostart.h"
#include "gui/thumbnailprovider.h"
#include "gui/viewer.h"
#include "kwiklooksettings.h"

#include <KLocalizedQmlContext>

#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFontMetricsF>
#include <QPointingDevice>
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

// The preview window as the app loads it, collecting QML warnings.
struct MainWindow {
    QQmlApplicationEngine engine;
    QList<QQmlError> errors;

    MainWindow()
    {
        KLocalization::setupLocalizedContext(&engine);
        engine.addImageProvider(QLatin1String(ThumbnailProvider::Name), new ThumbnailProvider);
        QObject::connect(&engine, &QQmlApplicationEngine::warnings, &engine, [this](const QList<QQmlError> &w) {
            errors << w;
        });
        engine.loadFromModule("kwiklook", "Main");
    }
    QObject *root() const
    {
        return engine.rootObjects().value(0);
    }
    QQuickWindow *window() const
    {
        return qobject_cast<QQuickWindow *>(root());
    }
    QString warnings() const
    {
        QStringList lines;
        for (const QQmlError &e : errors) {
            lines << e.toString();
        }
        return lines.join(u'\n');
    }
};

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
        MainWindow main;
        QCOMPARE(main.engine.rootObjects().size(), 1);
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
    }

    void loadsFallbackViewForAFile()
    {
        MainWindow main;
        QCOMPARE(main.engine.rootObjects().size(), 1);
        QTemporaryFile file(QDir::tempPath() + QStringLiteral("/kwiklook-XXXXXX.bin"));
        QVERIFY(file.open());
        file.write(QByteArray(3000, '\x07')); // binary: no view of its own
        file.flush();
        Viewer::instance()->Open({file.fileName()}, 0, QString(), QString());
        QTRY_COMPARE(Viewer::instance()->view(), QStringLiteral("FallbackView"));
        QTRY_VERIFY(main.root()->findChild<QQuickItem *>(QStringLiteral("fallbackView")));
        // One file: no position in the bar.
        QVERIFY(!main.root()->findChild<QQuickItem *>(u"position"_s)->isVisible());
        QTest::qWait(200); // let the thumbnail request finish or fail
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
    }

    void loadsImageViewWithInfoPanel()
    {
        MainWindow main;
        QCOMPARE(main.engine.rootObjects().size(), 1);
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("pic.png"));
        QImage img(40, 30, QImage::Format_RGB32);
        img.fill(Qt::green);
        QVERIFY(img.save(path));
        Viewer::instance()->setInfoOpen(true);
        Viewer::instance()->Open({path}, 0, QString(), QString());
        QObject *root = main.root();
        QTRY_VERIFY(root->findChild<QQuickItem *>(QStringLiteral("imageView")));
        QTRY_VERIFY(root->findChild<QQuickItem *>(QStringLiteral("infoPanel")));
        QTest::qWait(300);
        Viewer::instance()->setInfoOpen(false);
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
    }


    void playsAnimatedPictures()
    {
        MainWindow main;
        QObject *root = main.root();
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
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
    }

    void loadsTextViewAndKeysStillNavigate()
    {
        MainWindow main;
        QQuickWindow *window = main.window();
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
        // The position follows the file name.
        QQuickItem *position = window->findChild<QQuickItem *>(u"position"_s);
        QVERIFY(position);
        QVERIFY(position->isVisible());
        QCOMPARE(position->property("text").toString(), u"1 / 2"_s);
        QTest::keyClick(window, Qt::Key_Right);
        QTRY_COMPARE(v->currentIndex(), 1);
        QCOMPARE(position->property("text").toString(), u"2 / 2"_s);
        QTest::keyClick(window, Qt::Key_Left);
        QTRY_COMPARE(v->currentIndex(), 0);
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(!window->isVisible());
        v->setWindow(nullptr);
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
    }

    void textViewColoursScriptsAndKeepsLineNumbers()
    {
        MainWindow main;
        QQuickWindow *window = main.window();
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
        // The text has arrived (an empty area shows a "1" too, and loading the text
        // would undo the scrolling below).
        QTRY_VERIFY(shown().contains(u"20"_s));
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

    void longTextStaysResponsive()
    {
        // Like ~/.zcompdump: thousands of lines, some very long. The line numbers once
        // made a label per line of the whole text (the view was briefly that tall)
        // and froze the viewer.
        MainWindow main;
        QQuickWindow *window = main.window();
        QVERIFY(window);
        QFile f(m_keptDir.filePath(u"long.txt"_s));
        QVERIFY(f.open(QIODevice::WriteOnly));
        for (int i = 0; i < 3000; ++i) {
            f.write(i % 10 ? QByteArray("'word' '_completion'\n") : QByteArray(QByteArray(1000, 'x') + '\n'));
        }
        f.close();
        Viewer *v = Viewer::instance();
        v->setWindow(window);
        QElapsedTimer timer;
        timer.start();
        v->Open({f.fileName()}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QQuickItem *area = nullptr;
        QTRY_VERIFY((area = window->findChild<QQuickItem *>(u"textArea"_s)));
        QTRY_VERIFY(area->property("length").toInt() > 50000);
        QVERIFY2(timer.elapsed() < 5000, qPrintable(u"took %1 ms"_s.arg(timer.elapsed())));
        int labels = 0;
        for (QQuickItem *item : area->childItems()) {
            labels += item->objectName() == u"lineNumber"_s;
        }
        QVERIFY2(labels < 200, qPrintable(u"%1 line number labels"_s.arg(labels)));
        v->Close();
        v->setWindow(nullptr);
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
    }

    void loadsPdfView()
    {
        MainWindow main;
        QQuickWindow *window = main.window();
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
        QVERIFY(v->finishRendering()); // the engine goes next: see Viewer::finishRendering()
        v->Close();
        v->setWindow(nullptr);
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
    }

    void zoomsPicturesTextAndPdf()
    {
        MainWindow main;
        QQuickWindow *window = main.window();
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

        // As on Wayland: the mouse wheel comes from a device typed TouchPad.
        QPointingDevice waylandPointer(u"test pointer"_s, 1002, QInputDevice::DeviceType::TouchPad, QPointingDevice::PointerType::Finger,
                                       QInputDevice::Capability::Position | QInputDevice::Capability::Scroll, 1, 0);
        const auto ctrlWheel = [window, &waylandPointer](QQuickItem *over, int steps) {
            const QPointF pos = over->mapToScene(QPointF(over->width() / 2, over->height() / 2));
            QWheelEvent event(pos, window->mapToGlobal(pos), QPoint(), QPoint(0, 120 * steps), Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false, Qt::MouseEventNotSynthesized, &waylandPointer);
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
        // Plain scrolling moves the zoomed picture, it doesn't zoom (on Wayland from a
        // device typed TouchPad, also for a mouse wheel).
        QPointingDevice touchpad(u"test touchpad"_s, 1001, QInputDevice::DeviceType::TouchPad, QPointingDevice::PointerType::Finger,
                                 QInputDevice::Capability::Position | QInputDevice::Capability::Scroll | QInputDevice::Capability::PixelScroll, 1, 0);
        {
            const QPointF pos = image->mapToScene(QPointF(image->width() / 2, image->height() / 2));
            const qreal panY = image->property("panY").toReal();
            for (const auto &[phase, pixels] : {std::pair{Qt::ScrollBegin, 0}, {Qt::ScrollUpdate, -40}, {Qt::ScrollEnd, 0}}) {
                QWheelEvent scroll(pos, window->mapToGlobal(pos), QPoint(0, pixels), QPoint(0, pixels * 3), Qt::NoButton, Qt::NoModifier, phase, false, Qt::MouseEventNotSynthesized, &touchpad);
                QCoreApplication::sendEvent(window, &scroll);
            }
            QCOMPARE(image->property("zoom").toReal(), 8.0);
            QCOMPARE(image->property("panY").toReal(), panY - 40);
        }
        {
            const QPointF pos = image->mapToScene(QPointF(image->width() / 2, image->height() / 2));
            QWheelEvent wheel(pos, window->mapToGlobal(pos), QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false, Qt::MouseEventNotSynthesized, &touchpad);
            const qreal panY = image->property("panY").toReal();
            QCoreApplication::sendEvent(window, &wheel);
            QCOMPARE(image->property("zoom").toReal(), 8.0);
            QVERIFY(image->property("panY").toReal() < panY);
        }
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

        QVERIFY(v->finishRendering()); // the engine goes next: see Viewer::finishRendering()
        v->Close();
        QCOMPARE(v->property("textZoom").toReal(), 1.0);
        v->setWindow(nullptr);
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
    }

    void pdfPagesStayCentredAndInside()
    {
        MainWindow main;
        QQuickWindow *window = main.window();
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
        QVERIFY(v->finishRendering()); // the engine goes next: see Viewer::finishRendering()
        v->Close();
        v->setWindow(nullptr);
    }

    void officeDocumentBecomesAPdf()
    {
        if (!Viewer::instance()->officePreviewsAvailable() || !OfficeConverter::sandboxSupported()) {
            QSKIP("LibreOffice, kwiklook-sandbox or a kernel that can lock LibreOffice in is missing");
        }
        MainWindow main;
        QQuickWindow *window = main.window();
        QVERIFY(window);
        // A new name each run: not in the converter's cache yet.
        QFile doc(m_keptDir.filePath(u"report-%1.fodt"_s.arg(QDateTime::currentMSecsSinceEpoch())));
        QVERIFY(doc.open(QIODevice::WriteOnly));
        doc.write(R"(<?xml version="1.0" encoding="UTF-8"?>
<office:document xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
 xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0"
 office:version="1.3" office:mimetype="application/vnd.oasis.opendocument.text">
 <office:body><office:text><text:p>Quarterly report</text:p></office:text></office:body></office:document>
)");
        doc.close();
        Viewer *v = Viewer::instance();
        v->setWindow(window);
        v->Open({doc.fileName()}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        // Meanwhile: the fallback view with “Preparing preview…”.
        QTRY_VERIFY(v->previewBusy());
        QVERIFY2(window->findChild<QQuickItem *>(u"fallbackView"_s), qPrintable(v->view()));
        QCOMPARE(v->previewNote(), u"Preparing preview…"_s);
        // Then the PDF view, showing the converted PDF.
        QTRY_VERIFY_WITH_TIMEOUT(window->findChild<QQuickItem *>(u"pdfView"_s), 60000);
        QVERIFY(!v->previewBusy());
        QVERIFY(v->previewNote().isEmpty());
        QVERIFY(v->pdfUrl().toLocalFile().endsWith(u".pdf"_s));
        QTest::qWait(300);
        QVERIFY(v->finishRendering());
        v->Close();
        v->setWindow(nullptr);
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
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
        // Closing the preview stops it.
        v->Close();
        QTRY_COMPARE(player->property("playbackState").toInt(), 0); // StoppedState
        v->setWindow(nullptr);
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
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
