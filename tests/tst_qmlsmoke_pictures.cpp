// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>

// Smoke tests of the preview window, offscreen: pictures: fitting, SVG, animation, decoding ahead, zoom (also of text and PDFs).

#include "qmlsmoke.h"

class TstQmlSmokePictures : public QObject
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
        Viewer::instance()->setWindow(main.window()); // the view loads only while shown
        Viewer::instance()->Open({path}, 0, QString(), QString());
        QObject *root = main.root();
        QTRY_VERIFY(root->findChild<QQuickItem *>(QStringLiteral("imageView")));
        QTRY_VERIFY(root->findChild<QQuickItem *>(QStringLiteral("infoPanel")));
        QTest::qWait(300);
        Viewer::instance()->setInfoOpen(false);
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
        Viewer::instance()->Close();
        Viewer::instance()->setWindow(nullptr);
    }

    // Vector pictures fill the view (drawn again at that size); small pixel pictures stay
    // at real size; both edge to edge on the side that limits them.
    void vectorPicturesFillTheView()
    {
        MainWindow main;
        QQuickWindow *window = main.window();
        QVERIFY(window);
        QTemporaryDir dir;
        const QString svg = dir.filePath(u"small.svg"_s);
        QFile f(svg);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("<svg xmlns=\"http:/" "/www.w3.org/2000/svg\" width=\"100\" height=\"50\"><rect width=\"100\" height=\"50\" fill=\"teal\"/></svg>");
        f.close();
        const QString png = dir.filePath(u"small.png"_s);
        QImage img(100, 50, QImage::Format_RGB32);
        img.fill(Qt::darkCyan);
        QVERIFY(img.save(png));
        Viewer *v = Viewer::instance();
        v->setWindow(window);
        auto cleanup = qScopeGuard([v] {
            v->Close();
            v->setWindow(nullptr);
        });
        v->Open({svg, png}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QQuickItem *view = nullptr;
        QTRY_VERIFY((view = window->findChild<QQuickItem *>(u"imageView"_s)));
        QTRY_VERIFY(view->property("fit").toReal() > 1);
        const qreal fit = view->property("fit").toReal();
        QVERIFY(qFuzzyCompare(fit, std::min(view->width() / 100, view->height() / 50) * window->devicePixelRatio()));
        v->Next();
        QTRY_COMPARE(v->fileName(), u"small.png"_s);
        QTRY_COMPARE(view->property("fit").toReal(), 1.0);
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
        Viewer *v = Viewer::instance();
        v->setWindow(main.window());
        v->Open({gif.fileName()}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(main.window()));
        QQuickItem *animated = nullptr;
        QTRY_VERIFY((animated = root->findChild<QQuickItem *>(u"animatedImage"_s)));
        QTRY_COMPARE(animated->property("frameCount").toInt(), 2);
        QVERIFY(animated->property("playing").toBool());
        QVERIFY(!animated->property("paused").toBool());
        QVERIFY(animated->width() > 0);
        // Closed: paused at once (nothing runs unseen), and the view freed shortly after.
        const QPointer<QQuickItem> gone(animated);
        v->Close();
        QTRY_VERIFY(gone.isNull() || gone->property("paused").toBool());
        QTRY_VERIFY_WITH_TIMEOUT(gone.isNull(), 10000);
        v->setWindow(nullptr);
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
    }

    // The pictures next to the current file are decoded ahead: ← → shows them without
    // loading. Both kinds: one larger than the screen (decoded at screen size), one not.
    void picturesAreDecodedAhead()
    {
        MainWindow main;
        QQuickWindow *window = main.window();
        QVERIFY(window);
        QTemporaryDir dir;
        QStringList files;
        for (const QSize size : {QSize(64, 48), QSize(4000, 3000), QSize(120, 90)}) {
            QImage picture(size, QImage::Format_RGB32);
            picture.fill(Qt::darkCyan);
            files << dir.filePath(u"%1.png"_s.arg(size.width()));
            QVERIFY(picture.save(files.last()));
        }
        Viewer *v = Viewer::instance();
        v->setWindow(window);
        v->Open(files, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        // Repeater rows aren't children of the window: walk the items (hidden ones too).
        QList<QObject *> slots;
        QList<QQuickItem *> todo{window->contentItem()};
        while (!todo.isEmpty()) {
            QQuickItem *item = todo.takeFirst();
            if (item->objectName() == u"pictureAhead"_s) {
                slots << item;
            }
            todo << item->childItems();
        }
        QCOMPARE(slots.size(), 2);
        const auto ahead = [&slots](const QString &file) {
            for (QObject *slot : slots) {
                if (slot->property("source").toUrl() == QUrl::fromLocalFile(file) && slot->property("status").toInt() == 1) {
                    return true;
                }
            }
            return false;
        };
        for (int i = 1; i < files.size(); ++i) {
            QTRY_VERIFY2(ahead(files.at(i)), qPrintable(files.at(i)));
            // Settled: an idle slot doesn't load again.
            QList<std::shared_ptr<QSignalSpy>> spies;
            for (QObject *slot : slots) {
                spies << std::make_shared<QSignalSpy>(slot, SIGNAL(statusChanged(QQuickImageBase::Status)));
            }
            QTest::qWait(300);
            for (const auto &spy : spies) {
                QCOMPARE(spy->count(), 0);
            }
            v->Next();
            QTRY_COMPARE(v->currentIndex(), i);
            QQuickItem *view = nullptr;
            QTRY_VERIFY((view = findItem(window, u"imageView"_s)));
            QObject *image = view->property("image").value<QObject *>();
            QTRY_COMPARE(image->property("source").toUrl(), QUrl::fromLocalFile(files.at(i)));
            QCOMPARE(image->property("status").toInt(), 1); // Ready at once: from the cache
        }
        // Closed: the slots let go of their pictures.
        v->Close();
        for (QObject *slot : slots) {
            QTRY_COMPARE(slot->property("source").toUrl(), QUrl());
        }
        v->setWindow(nullptr);
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
        QObject *indicator = window->findChild<QObject *>(u"zoomLevel"_s);
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
        QTRY_VERIFY(indicator->property("visible").toBool()); // in the corner box
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
};

QTEST_MAIN(TstQmlSmokePictures)
#include "tst_qmlsmoke_pictures.moc"
