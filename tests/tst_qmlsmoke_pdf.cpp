// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>

// Smoke tests of the preview window, offscreen: PDFs and office documents: pages, the sidebar and contents, find.

#include "qmlsmoke.h"

class TstQmlSmokePdf : public QObject
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

    void pdfPageOverview()
    {
        MainWindow main;
        QQuickWindow *window = main.window();
        QVERIFY(window);
        QTemporaryDir dir;
        const QString pdf = dir.filePath(u"three.pdf"_s);
        const QString single = dir.filePath(u"one.pdf"_s);
        {
            QPdfWriter writer(pdf);
            QPainter painter(&writer);
            for (int i = 1; i <= 3; ++i) {
                painter.drawText(QPointF(100, 100), u"Page %1"_s.arg(i));
                if (i < 3) {
                    writer.newPage();
                }
            }
        }
        {
            QPdfWriter writer(single);
            QPainter painter(&writer);
            painter.drawText(QPointF(100, 100), u"Only page"_s);
        }
        Viewer *v = Viewer::instance();
        const bool wasOpen = v->pagesOpen();
        v->setPagesOpen(false);
        v->setWindow(window);
        v->Open({pdf, single}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QQuickItem *pdfView = nullptr;
        QTRY_VERIFY((pdfView = window->findChild<QQuickItem *>(u"pdfView"_s)));
        QTRY_COMPARE(pdfView->property("pageCount").toInt(), 3);
        // The bar's button opens the overview, remembered.
        QQuickItem *button = findItem(window, u"barPages"_s);
        QVERIFY(button && button->isVisible());
        QMetaObject::invokeMethod(button, "clicked");
        QVERIFY(v->pagesOpen());
        QQuickItem *overview = window->findChild<QQuickItem *>(u"pageOverview"_s);
        QTRY_VERIFY(overview->isVisible() && overview->width() > 0);
        QList<QQuickItem *> thumbs;
        QTRY_VERIFY([&] {
            thumbs.clear();
            QList<QQuickItem *> todo{overview};
            while (!todo.isEmpty()) {
                QQuickItem *item = todo.takeFirst();
                if (item->objectName() == u"pageThumbnail"_s) {
                    thumbs << item;
                }
                todo << item->childItems();
            }
            return thumbs.size() == 3;
        }());
        // A click on the third page goes there.
        QCOMPARE(pdfView->property("currentPage").toInt(), 0);
        QMetaObject::invokeMethod(pdfView, "showPage", Q_ARG(QVariant, 2));
        QTRY_COMPARE(pdfView->property("currentPage").toInt(), 2);
        // A one-page PDF: no button, no overview.
        v->Next();
        QTRY_COMPARE(pdfView->property("pageCount").toInt(), 1);
        QTRY_VERIFY(!button->isVisible());
        QTRY_VERIFY(!overview->isVisible());
        QVERIFY(v->finishRendering());
        v->Close();
        v->setWindow(nullptr);
        v->setPagesOpen(wasOpen);
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
    }

    void contentsSidebar()
    {
        MainWindow main;
        QQuickWindow *window = main.window();
        QVERIFY(window);
        QTemporaryDir dir;
        const QString pdf = dir.filePath(u"book.pdf"_s);
        writeBookmarkedPdf(pdf);
        QFile md(dir.filePath(u"notes.md"_s));
        QVERIFY(md.open(QIODevice::WriteOnly));
        QByteArray text = "# One\n\nintro\n\n```\n# not a heading\n```\n\nTwo\n---\n";
        for (int i = 0; i < 200; ++i) {
            text += "Filler line " + QByteArray::number(i) + "\n\n";
        }
        text += "### Three *last*\n\nend\n";
        md.write(text);
        md.close();
        const QString single = dir.filePath(u"one.pdf"_s);
        {
            QPdfWriter writer(single);
            QPainter painter(&writer);
            painter.drawText(QPointF(100, 100), u"Only page"_s);
        }
        Viewer *v = Viewer::instance();
        const bool wasOpen = v->pagesOpen();
        const QString wasTab = v->sidebarTab();
        v->setPagesOpen(false);
        v->setSidebarTab(u"contents"_s);
        v->setWindow(window);
        v->Open({pdf, md.fileName(), single}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));

        // The PDF's bookmarks, flattened.
        QQuickItem *pdfView = nullptr;
        QTRY_VERIFY((pdfView = window->findChild<QQuickItem *>(u"pdfView"_s)));
        QTRY_COMPARE(pdfView->property("contents").toList().size(), 3);
        const QVariantList entries = pdfView->property("contents").toList();
        QCOMPARE(entries.at(0).toMap().value(u"title"_s).toString(), u"Chapter 1"_s);
        QCOMPARE(entries.at(1).toMap().value(u"title"_s).toString(), u"Section 1.1"_s);
        QCOMPARE(entries.at(1).toMap().value(u"level"_s).toInt(), 2);
        QCOMPARE(entries.at(1).toMap().value(u"page"_s).toInt(), 0);
        QCOMPARE(entries.at(1).toMap().value(u"y"_s).toReal(), 400.0); // from the page's top
        QCOMPARE(entries.at(2).toMap().value(u"page"_s).toInt(), 2);
        // The bar's button opens the sidebar on Contents (chosen before), with tabs.
        QQuickItem *button = findItem(window, u"barPages"_s);
        QVERIFY(button && button->isVisible());
        QMetaObject::invokeMethod(button, "clicked");
        QQuickItem *sidebar = window->findChild<QQuickItem *>(u"pageOverview"_s);
        QTRY_VERIFY(sidebar->isVisible() && sidebar->width() > 0);
        QVERIFY(window->findChild<QQuickItem *>(u"sidebarTabs"_s)->isVisible());
        QTRY_COMPARE(itemsNamed(sidebar, u"contentsEntry"_s).size(), 3);
        QCOMPARE(itemsNamed(sidebar, u"contentsFold"_s).size(), 1); // Chapter 1 has a section
        // Folded, Chapter 1 hides its section, and stands for it when that's current.
        QObject *tree = sidebar->findChild<QObject *>(u"contentsTree"_s);
        QMetaObject::invokeMethod(tree, "toggle", Q_ARG(QVariant, 0));
        QTRY_COMPARE(itemsNamed(sidebar, u"contentsEntry"_s).size(), 2);
        tree->setProperty("currentIndex", 1);
        QCOMPARE(tree->property("currentEntry").toInt(), 0);
        QMetaObject::invokeMethod(tree, "toggle", Q_ARG(QVariant, 0));
        QTRY_COMPARE(itemsNamed(sidebar, u"contentsEntry"_s).size(), 3);
        QCOMPARE(tree->property("currentEntry").toInt(), 1);
        // The current entry follows the view; going to an entry scrolls there.
        QTRY_COMPARE(pdfView->property("currentContentsEntry").toInt(), 0);
        QMetaObject::invokeMethod(pdfView, "showDestination", Q_ARG(QVariant, 0), Q_ARG(QVariant, 400.0));
        QTRY_COMPARE(pdfView->property("currentContentsEntry").toInt(), 1);
        QQuickItem *chapter2 = itemsNamed(sidebar, u"contentsEntry"_s).at(2);
        QMetaObject::invokeMethod(chapter2, "clicked");
        QTRY_COMPARE(pdfView->property("currentContentsEntry").toInt(), 2);
        QTRY_COMPARE(pdfView->property("currentPage").toInt(), 2);
        // The two tabs share the bar evenly; the sidebar keeps its width on both tabs.
        QQuickItem *tabs = window->findChild<QQuickItem *>(u"sidebarTabs"_s);
        QQuickItem *pagesTab = window->findChild<QQuickItem *>(u"pagesTab"_s);
        QQuickItem *contentsTab = window->findChild<QQuickItem *>(u"contentsTab"_s);
        QTRY_COMPARE(sidebar->width(), tabs->width());
        const qreal sidebarWidth = sidebar->width();
        QCOMPARE(pagesTab->width(), tabs->width() / 2);
        QCOMPARE(contentsTab->width(), tabs->width() / 2);
        // The Pages tab shows the small pages, and is remembered.
        QMetaObject::invokeMethod(pagesTab, "clicked");
        QCOMPARE(v->sidebarTab(), u"pages"_s);
        QTRY_COMPARE(itemsNamed(sidebar, u"pageThumbnail"_s).size(), 3);
        QCOMPARE(sidebar->width(), sidebarWidth);
        QTRY_COMPARE(itemsNamed(sidebar, u"contentsEntry"_s).size(), 0);

        // Markdown: its headings in the sidebar (no tabs), rendered and as source.
        v->Next();
        QQuickItem *textView = nullptr;
        QTRY_VERIFY((textView = window->findChild<QQuickItem *>(u"textView"_s)));
        QTRY_COMPARE(textView->property("contents").toList().size(), 3);
        QStringList titles;
        for (const QVariant &e : textView->property("contents").toList()) {
            titles << e.toMap().value(u"title"_s).toString();
        }
        QCOMPARE(titles, (QStringList{u"One"_s, u"Two"_s, u"Three last"_s}));
        QQuickItem *textSidebar = window->findChild<QQuickItem *>(u"textContents"_s);
        QTRY_VERIFY(textSidebar->isVisible() && textSidebar->width() > 0);
        QTRY_COMPARE(itemsNamed(textSidebar, u"contentsEntry"_s).size(), 3);
        QCOMPARE(textView->property("currentContentsEntry").toInt(), 0);
        QMetaObject::invokeMethod(itemsNamed(textSidebar, u"contentsEntry"_s).at(2), "clicked");
        QTRY_COMPARE(textView->property("currentContentsEntry").toInt(), 2);
        // Source: the same headings, at their lines.
        v->setShowFormatted(false);
        QQuickItem *area = window->findChild<QQuickItem *>(u"textArea"_s);
        QTRY_COMPARE(area->property("textFormat").toInt(), int(Qt::PlainText));
        const auto sourceAt = [textView](int i) {
            return textView->property("contents").toList().value(i).toMap();
        };
        QTRY_COMPARE(sourceAt(1).value(u"position"_s).toInt(), int(QByteArray(text).indexOf("Two\n---")));
        const QVariantList source = textView->property("contents").toList();
        QCOMPARE(source.size(), 3);
        QCOMPARE(source.at(0).toMap().value(u"title"_s).toString(), u"One"_s);
        QCOMPARE(source.at(2).toMap().value(u"position"_s).toInt(), int(QByteArray(text).indexOf("### Three")));
        QMetaObject::invokeMethod(textView, "showContentsEntry", Q_ARG(QVariant, 2));
        QTRY_COMPARE(textView->property("currentContentsEntry").toInt(), 2);
        QMetaObject::invokeMethod(textView, "showContentsEntry", Q_ARG(QVariant, 0));
        QTRY_COMPARE(textView->property("currentContentsEntry").toInt(), 0);
        v->setShowFormatted(true);

        // One page, no bookmarks: no button, no sidebar.
        v->Next();
        QTRY_VERIFY((pdfView = window->findChild<QQuickItem *>(u"pdfView"_s)));
        QTRY_COMPARE(pdfView->property("pageCount").toInt(), 1);
        QTRY_VERIFY(!button->isVisible());
        QTRY_VERIFY(!window->findChild<QQuickItem *>(u"pageOverview"_s)->isVisible());
        QVERIFY(v->finishRendering());
        v->Close();
        v->setWindow(nullptr);
        v->setPagesOpen(wasOpen);
        v->setSidebarTab(wasTab);
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
    }

    void findsInPdfsAndKeepsTheSearch()
    {
        MainWindow main;
        QQuickWindow *window = main.window();
        QVERIFY(window);
        QTemporaryDir dir;
        QFile txt(dir.filePath(u"list.txt"_s));
        QVERIFY(txt.open(QIODevice::WriteOnly));
        txt.write("one apple\n");
        txt.close();
        const QString pdf = dir.filePath(u"fruit.pdf"_s);
        {
            QPdfWriter writer(pdf);
            QPainter painter(&writer);
            painter.drawText(QPointF(100, 100), u"An apple a day"_s);
            writer.newPage();
            painter.drawText(QPointF(100, 100), u"No fruit here"_s);
            writer.newPage();
            painter.drawText(QPointF(100, 100), u"Apple pie"_s);
            painter.drawText(QPointF(100, 400), u"apple tree"_s);
        }
        Viewer *v = Viewer::instance();
        v->setWindow(window);
        v->Open({txt.fileName(), pdf}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTRY_VERIFY(window->findChild<QQuickItem *>(u"textArea"_s));
        v->setProperty("searchOpen", true);
        v->setProperty("searchText", u"apple"_s);
        QQuickItem *count = nullptr;
        QTRY_VERIFY((count = window->findChild<QQuickItem *>(u"findCount"_s)));
        QTRY_COMPARE(count->property("text").toString(), u"1 of 1"_s);
        // The next file keeps the bar and the search: the PDF finds its three, on pages 1 and 3.
        v->Next();
        QQuickItem *pdfView = nullptr;
        QTRY_VERIFY((pdfView = window->findChild<QQuickItem *>(u"pdfView"_s)));
        QTRY_VERIFY((count = pdfView->findChild<QQuickItem *>(u"findCount"_s)));
        QTRY_COMPARE(count->property("text").toString(), u"1 of 3"_s);
        // The search model exists only while the find bar is open (it wakes the viewer).
        const auto searchModel = [pdfView] {
            QObject *found = nullptr;
            for (QObject *o : pdfView->findChildren<QObject *>()) {
                if (o->inherits("QPdfSearchModel")) {
                    found = o;
                }
            }
            return found;
        };
        v->setProperty("searchOpen", false);
        QTRY_VERIFY(!searchModel());
        v->setProperty("searchOpen", true);
        QTRY_COMPARE(count->property("text").toString(), u"1 of 3"_s);
        QMetaObject::invokeMethod(pdfView, "findNext");
        QMetaObject::invokeMethod(pdfView, "findNext");
        QCOMPARE(count->property("text").toString(), u"3 of 3"_s);
        QObject *search = searchModel();
        QVERIFY(search);
        QCOMPARE(search->property("currentPage").toInt(), 2);
        QMetaObject::invokeMethod(pdfView, "findNext"); // round to the first
        QCOMPARE(count->property("text").toString(), u"1 of 3"_s);
        QCOMPARE(search->property("currentPage").toInt(), 0);
        QVERIFY(v->finishRendering());
        v->Close();
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

        QQuickItem *list = pdfView->findChild<QQuickItem *>(u"pdfPages"_s);
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
        QQuickItem *pageNumber = findItem(window, u"pageNumber"_s);
        QVERIFY(pageNumber && pageNumber->opacity() == 0); // not when the document opens
        for (int i = 0; i < 20; ++i) { // scroll down, pages get created and reused
            QMetaObject::invokeMethod(list, "flick", Q_ARG(qreal, 0), Q_ARG(qreal, -3000));
            QTest::qWait(50);
        }
        QVERIFY(pageNumber->isVisible()); // while scrolling: “5 / 6”
        QVERIFY2(pageNumber->property("text").toString().endsWith(u" / 6"_s), qPrintable(pageNumber->property("text").toString()));
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
};

QTEST_MAIN(TstQmlSmokePdf)
#include "tst_qmlsmoke_pdf.moc"
