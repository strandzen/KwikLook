// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>

// Smoke tests of the preview window, offscreen: text, code, Markdown, formatted views, find in text, books.

#include "qmlsmoke.h"

class TstQmlSmokeText : public QObject
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
        // Where you are shows in the corner on each switch, then fades.
        QQuickItem *position = window->findChild<QQuickItem *>(u"position"_s);
        QQuickItem *corner = window->findChild<QQuickItem *>(u"cornerLabel"_s);
        QVERIFY(position && corner);
        QCOMPARE(position->property("text").toString(), u"1 / 2"_s);
        QTRY_VERIFY_WITH_TIMEOUT(!corner->isVisible(), 3000);
        QTest::keyClick(window, Qt::Key_Right);
        QTRY_COMPARE(v->currentIndex(), 1);
        QCOMPARE(position->property("text").toString(), u"2 / 2"_s);
        QTRY_VERIFY(corner->isVisible() && position->isVisible());
        QTest::keyClick(window, Qt::Key_Left);
        QTRY_COMPARE(v->currentIndex(), 0);
        // ↑ ↓ do the same, also inside the text.
        QTest::keyClick(window, Qt::Key_Down);
        QTRY_COMPARE(v->currentIndex(), 1);
        QTest::keyClick(window, Qt::Key_Up);
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
        // Scrolled again while the gutter widens for three digits (the text area then
        // scrolls back to its cursor, once).
        const qreal target = 100 * QFontMetricsF(area->property("font").value<QFont>()).lineSpacing();
        QTRY_VERIFY2((flickable->property("contentY").toReal() == target || flickable->setProperty("contentY", target))
                         && !shown().contains(u"1"_s) && shown().contains(u"110"_s),
                     qPrintable(u"shown %1; contentY %2, contentHeight %3, height %4, visible %5"_s.arg(shown().join(u","_s))
                                    .arg(flickable->property("contentY").toReal())
                                    .arg(flickable->property("contentHeight").toReal())
                                    .arg(flickable->property("height").toReal())
                                    .arg(window->isVisible())));
        QCOMPARE(labels(), before);
        v->Close();
        v->setWindow(nullptr);
    }

    // KDE's highlighter re-colours the next line a moment later when a line's folding
    // changes; a new file's text must not arrive while such a call waits (it crashed:
    // the file name, and with it the rules, changed before the text did).
    void switchingAwayFromCodeDoesNotCrash()
    {
        MainWindow main;
        QQuickWindow *window = main.window();
        QVERIFY(window);
        QTemporaryDir dir;
        const auto write = [&dir](const QString &name, const QByteArray &text) {
            QFile f(dir.filePath(name));
            if (!f.open(QIODevice::WriteOnly)) {
                return QString();
            }
            f.write(text);
            return f.fileName();
        };
        QByteArray code;
        QByteArray python;
        for (int i = 0; i < 40; ++i) {
            code += "void f" + QByteArray::number(i) + "()\n{\n    if (x) {\n        y();\n    }\n}\n";
            python += "def f" + QByteArray::number(i) + "():\n    if x:\n        y()\n\n";
        }
        const QString cpp = write(u"code.cpp"_s, code);
        const QString py = write(u"script.py"_s, python);
        const QString png = dir.filePath(u"picture.png"_s);
        QImage picture(64, 48, QImage::Format_RGB32);
        picture.fill(Qt::darkCyan);
        QVERIFY(picture.save(png));
        Viewer *v = Viewer::instance();
        v->setWindow(window);
        for (int round = 0; round < 3; ++round) {
            // Shown: switches while the window is open.
            v->Open({cpp, py, png, cpp}, 0, QString(), QString());
            QVERIFY(QTest::qWaitForWindowExposed(window));
            QTRY_VERIFY(v->text().startsWith(u"void f0"_s));
            for (int i = 0; i < 3; ++i) {
                v->Next();
                QTRY_VERIFY(!v->switching());
                QTest::qWait(50);
            }
            // Hidden: the next opening starts on another kind of file.
            v->Close();
            QTRY_VERIFY(!window->isVisible());
            v->Open({png, cpp}, 0, QString(), QString());
            QTRY_VERIFY(!v->switching());
            QTest::qWait(50);
            v->Close();
            QTRY_VERIFY(!window->isVisible());
        }
        v->setWindow(nullptr);
    }

    void markdownRendersWithoutFetching()
    {
        // Stands in for a web server: no connection may arrive.
        QTcpServer web;
        QVERIFY(web.listen(QHostAddress::LocalHost));
        MainWindow main;
        QQuickWindow *window = main.window();
        QVERIFY(window);
        QTemporaryDir dir;
        QFile f(dir.filePath(u"notes.md"_s));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(u"# Title\n\nSome *text* and a [link](https://kde.org).\n\n![remote](http://127.0.0.1:%1/picture.png)\n\n<img src=\"http://127.0.0.1:%1/html.png\">\n"_s
                    .arg(web.serverPort())
                    .toUtf8());
        f.close();
        Viewer *v = Viewer::instance();
        KSharedConfig::openStateConfig()->group(u"FormattedView"_s).deleteGroup(); // all formatted
        v->setWindow(window);
        v->Open({f.fileName()}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QQuickItem *area = nullptr;
        QTRY_VERIFY((area = window->findChild<QQuickItem *>(u"textArea"_s)));
        QTRY_COMPARE(area->property("textFormat").toInt(), int(Qt::MarkdownText));
        QVERIFY(!area->findChild<QObject *>(u"highlighter"_s)); // no source colours over it
        QTRY_VERIFY(!area->property("text").toString().isEmpty());
        // Pictures refused (Qt's Markdown may drop the HTML one), and nothing reached the server.
        QTRY_VERIFY(main.warnings().count(u"Protocol \"%1\" is unknown"_s.arg(QLatin1String(OfflineNetworkFactory::BlockedScheme))) >= 1);
        QTest::qWait(200);
        QVERIFY2(!web.hasPendingConnections(), "the preview fetched a picture from the network");
        main.errors.clear();
        // Source: plain text with colours; the choice is remembered.
        // The bar's toggle shows for Markdown only.
        QQuickItem *toggle = findItem(window, u"barViewMode"_s);
        QVERIFY(toggle && toggle->isVisible());
        QCOMPARE(toggle->property("checked").toBool(), true);
        QMetaObject::invokeMethod(toggle, "clicked");
        QCOMPARE(v->showFormatted(), false);
        QTRY_COMPARE(area->property("textFormat").toInt(), int(Qt::PlainText));
        QTRY_VERIFY(area->findChild<QObject *>(u"highlighter"_s));
        KConfig state(KSharedConfig::openStateConfig()->name(), KConfig::SimpleConfig, QStandardPaths::GenericStateLocation);
        QCOMPARE(state.group(u"FormattedView"_s).readEntry("markdown", true), false);
        // The viewer is shared: leave a plain file current, or the next test's new
        // window renders notes.md first (and logs its refused pictures).
        QFile plain(m_keptDir.filePath(u"after-markdown.txt"_s));
        QVERIFY(plain.open(QIODevice::WriteOnly));
        plain.close();
        v->Open({plain.fileName()}, 0, QString(), QString());
        QTRY_VERIFY(!toggle->isVisible());
        v->Close();
        v->setWindow(nullptr);
        KSharedConfig::openStateConfig()->group(u"FormattedView"_s).deleteGroup();
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
    }

    // A README that starts with a centred logo (<img> without its slash): Qt's reader
    // dropped everything after it, and the contents showed “(Untitled)”.
    void markdownWithAPictureHeader()
    {
        MainWindow main;
        QQuickWindow *window = main.window();
        QVERIFY(window);
        // Kept: the shared viewer may still load the picture after this test.
        QDir dir(m_keptDir.filePath(u"readme"_s));
        QVERIFY(dir.mkpath(u"."_s));
        QImage icon(8, 8, QImage::Format_RGB32);
        icon.fill(Qt::darkBlue);
        QVERIFY(icon.save(dir.filePath(u"icon.png"_s)));
        QFile f(dir.filePath(u"README.md"_s));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("<div align=\"center\">\n    <img width=\"64\" src=\"icon.png\">\n    <h1><b>Project</b></h1>\n</div>\n\n"
                "Intro text.\n\n## Features\n\nMore text.\n\n## Usage\n\nThe end.\n");
        f.close();
        Viewer *v = Viewer::instance();
        v->setWindow(window);
        v->Open({f.fileName()}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTRY_COMPARE(v->formattedKind(), u"markdown"_s);
        const bool wasFormatted = v->showFormatted();
        QQuickItem *textView = nullptr;
        QTRY_VERIFY((textView = window->findChild<QQuickItem *>(u"textView"_s)));
        const auto titles = [textView] {
            QStringList found;
            for (const QVariant &entry : textView->property("contents").toList()) {
                found << entry.toMap().value(u"title"_s).toString();
            }
            return found;
        };
        const QStringList expected{u"Project"_s, u"Features"_s, u"Usage"_s};
        for (const bool formatted : {true, false}) {
            v->setShowFormatted(formatted);
            QTRY_COMPARE(titles(), expected);
            QQuickItem *area = window->findChild<QQuickItem *>(u"textArea"_s);
            QTRY_VERIFY(area->property("text").toString().contains(u"The end."_s));
            if (formatted) { // the text after the picture is there, rendered
                QVERIFY(area->property("textDocument").value<QQuickTextDocument *>()->textDocument()->toPlainText().contains(u"The end."_s));
            } else { // the source as written, without the added slash
                QVERIFY(area->property("text").toString().contains(u"src=\"icon.png\">"_s));
            }
        }
        v->setShowFormatted(wasFormatted);
        QVERIFY2(main.warnings().isEmpty(), qPrintable(main.warnings()));
        // Next, a Markdown file in another folder (a picture of its own): the old
        // text's picture must not be looked for in the new folder during the switch,
        // nor the new one's in the old folder.
        QDir other(m_keptDir.filePath(u"other"_s));
        QVERIFY(other.mkpath(u"."_s));
        QVERIFY(icon.save(other.filePath(u"logo.png"_s)));
        QFile next(other.filePath(u"NEXT.md"_s));
        QVERIFY(next.open(QIODevice::WriteOnly));
        next.write("# Next\n\n![logo](logo.png)\n\nNext text.\n");
        next.close();
        v->setShowFormatted(true);
        v->Open({f.fileName(), next.fileName()}, 0, QString(), QString());
        QTRY_VERIFY(v->text().contains(u"The end."_s));
        v->Next();
        QTRY_VERIFY(v->text().contains(u"Next text."_s));
        QTest::qWait(300); // pictures load after the layout
        v->Previous();
        QTRY_VERIFY(v->text().contains(u"The end."_s));
        QTest::qWait(300);
        v->setShowFormatted(wasFormatted);
        QVERIFY2(main.warnings().isEmpty(), qPrintable(main.warnings()));
        v->Close();
        v->setWindow(nullptr);
    }

    void findsInText()
    {
        MainWindow main;
        QQuickWindow *window = main.window();
        QVERIFY(window);
        QFile f(m_keptDir.filePath(u"fruit.txt"_s));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("apple banana\nApple cherry\napple\n");
        f.close();
        Viewer *v = Viewer::instance();
        v->setWindow(window);
        v->Open({f.fileName()}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTRY_VERIFY(window->findChild<QQuickItem *>(u"textArea"_s));
        QQuickItem *text = window->findChild<QQuickItem *>(u"textView"_s);
        QTRY_COMPARE(text->property("matches").toList().size(), 0);
        // Ctrl+F opens the bar with the field focused; typing finds, ignoring case.
        QTest::keyClick(window, Qt::Key_F, Qt::ControlModifier);
        QVERIFY(v->property("searchOpen").toBool());
        auto *field = window->findChild<QQuickItem *>(u"findField"_s);
        QVERIFY(field);
        QTRY_VERIFY(field->hasActiveFocus());
        for (const char c : {'a', 'p', 'p', 'l', 'e'}) {
            QTest::keyClick(window, c);
        }
        QCOMPARE(v->property("searchText").toString(), u"apple"_s);
        QQuickItem *count = window->findChild<QQuickItem *>(u"findCount"_s);
        QTRY_COMPARE(count->property("text").toString(), u"1 of 3"_s);
        QCOMPARE(text->property("matches").toList(), QVariantList({0, 13, 26}));
        // Enter: the next match, not “open the file”.
        QTest::keyClick(window, Qt::Key_Return);
        QCOMPARE(count->property("text").toString(), u"2 of 3"_s);
        QTest::keyClick(window, Qt::Key_Return, Qt::ShiftModifier);
        QCOMPARE(count->property("text").toString(), u"1 of 3"_s);
        QVERIFY(window->isVisible());
        // Marks on screen, one per match.
        int marks = 0;
        QList<QQuickItem *> todo{window->contentItem()};
        while (!todo.isEmpty()) {
            QQuickItem *item = todo.takeFirst();
            marks += item->objectName() == u"matchMark"_s && item->isVisible();
            todo << item->childItems();
        }
        QCOMPARE(marks, 3);
        // Not found.
        QTest::keyClick(window, 'x');
        QTRY_COMPARE(count->property("text").toString(), u"Not found"_s);
        // Esc closes the bar, the next Esc the preview; closing forgets the search.
        QTest::keyClick(window, Qt::Key_Escape);
        QVERIFY(!v->property("searchOpen").toBool());
        QVERIFY(window->isVisible());
        QTRY_COMPARE(text->property("matches").toList().size(), 0);
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!window->isVisible());
        QCOMPARE(v->property("searchText").toString(), QString());
        v->setWindow(nullptr);
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
    }

    void formattedViewsAndTheirToggle()
    {
        KSharedConfig::openStateConfig()->group(u"FormattedView"_s).deleteGroup();
        const auto restore = qScopeGuard([] {
            KSharedConfig::openStateConfig()->group(u"FormattedView"_s).deleteGroup();
        });
        MainWindow main;
        QQuickWindow *window = main.window();
        QVERIFY(window);
        QTemporaryDir dir;
        const auto write = [&dir](const QString &name, const QByteArray &data) {
            QFile f(dir.filePath(name));
            if (f.open(QIODevice::WriteOnly)) {
                f.write(data);
            }
            f.close();
            return f.fileName();
        };
        const QStringList files{
            write(u"pic.svg"_s, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"40\" height=\"40\"><rect width=\"40\" height=\"40\" fill=\"red\"/></svg>"),
            write(u"data.json"_s, "{\"a\":[1,2]}"),
            write(u"page.html"_s, "<html><body><h1>Title</h1><p>Text</p></body></html>\n"),
            write(u"app.desktop"_s, "[Desktop Entry]\nType=Application\nName=Example App\nExec=example\n"),
            write(u"table.csv"_s, "fruit,count\napple,3\npear,5\n"),
            write(u"people.vcf"_s, "BEGIN:VCARD\nVERSION:3.0\nFN:Ada Lovelace\nN:Lovelace;Ada;;;\nEND:VCARD\n"),
        };
        Viewer *v = Viewer::instance();
        v->setWindow(window);
        v->Open(files, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        // SVG: the picture; the toggle shows its text, and back.
        QTRY_VERIFY(window->findChild<QQuickItem *>(u"imageView"_s));
        QQuickItem *toggle = findItem(window, u"barViewMode"_s);
        QVERIFY(toggle && toggle->isVisible());
        QMetaObject::invokeMethod(toggle, "clicked");
        QTRY_VERIFY(window->findChild<QQuickItem *>(u"textArea"_s));
        QTRY_VERIFY(!window->findChild<QQuickItem *>(u"imageView"_s)); // deleted later
        // JSON (its own choice: formatted): indented.
        v->Next();
        QQuickItem *area = nullptr;
        QTRY_VERIFY((area = window->findChild<QQuickItem *>(u"textArea"_s)) && area->property("text").toString().contains(u"\n    \"a\""_s));
        QVERIFY(v->showFormatted());
        // HTML: rich text.
        v->Next();
        QTRY_COMPARE(area->property("textFormat").toInt(), int(Qt::RichText));
        // .desktop: the card.
        v->Next();
        QTRY_VERIFY(window->findChild<QQuickItem *>(u"desktopView"_s));
        QQuickItem *name = window->findChild<QQuickItem *>(u"desktopName"_s);
        QVERIFY(name);
        QCOMPARE(name->property("text").toString(), u"Example App"_s);
        // CSV: a table, the first row as headers.
        v->Next();
        QTRY_VERIFY(window->findChild<QQuickItem *>(u"csvView"_s));
        QCOMPARE(v->table()->rowCount(), 2);
        QCOMPARE(v->table()->columnCount(), 2);
        QCOMPARE(v->table()->headerData(1, Qt::Horizontal).toString(), u"count"_s);
        QCOMPARE(v->table()->index(1, 0).data().toString(), u"pear"_s);
        // vCard: a card per contact.
        v->Next();
        QTRY_VERIFY(window->findChild<QQuickItem *>(u"cardsView"_s));
        QQuickItem *title = nullptr;
        QTRY_VERIFY((title = findItem(window, u"cardTitle"_s)));
        QCOMPARE(title->property("text").toString(), u"Ada Lovelace"_s);
        v->Previous();
        v->Previous();
        // Back to the SVG: still its text (remembered for SVG only).
        v->Previous();
        v->Previous();
        v->Previous();
        QTRY_VERIFY(window->findChild<QQuickItem *>(u"textArea"_s) && !window->findChild<QQuickItem *>(u"imageView"_s));
        QVERIFY(!v->showFormatted());
        v->Close();
        v->setWindow(nullptr);
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
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

    // While shown, the next file replaces the current one in one step, after its details
    // are read; the old view stays as a still picture and fades out.
    // An EPUB book: unpacked in the sandbox, shown as formatted text with its own
    // table of contents in the sidebar and its pictures; no formatted / source toggle.
    void showsAnEpubBook()
    {
        if (!Sandbox::supported()) {
            QSKIP("this kernel's Landlock can't lock the archive helper in (books are read in the sandbox)");
        }
        const QString bsdtar = QStandardPaths::findExecutable(u"bsdtar"_s);
        if (bsdtar.isEmpty()) {
            QSKIP("bsdtar is needed to make the test book");
        }
        MainWindow main;
        QQuickWindow *window = main.window();
        QVERIFY(window);
        QTemporaryDir dir;
        const auto write = [&dir](const QString &name, const QByteArray &data) {
            const QString path = dir.filePath(u"book/"_s + name);
            QDir().mkpath(QFileInfo(path).absolutePath());
            QFile f(path);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(data);
            }
        };
        write(u"mimetype"_s, "application/epub+zip");
        write(u"META-INF/container.xml"_s, "<container><rootfiles><rootfile full-path=\"book.opf\"/></rootfiles></container>");
        write(u"book.opf"_s,
              "<package xmlns=\"http://www.idpf.org/2007/opf\"><metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\"><dc:title>Tiny Book</dc:title>"
              "</metadata><manifest><item id=\"toc\" href=\"toc.ncx\"/><item id=\"a\" href=\"a.xhtml\"/><item id=\"b\" href=\"b.xhtml\"/>"
              "</manifest><spine toc=\"toc\"><itemref idref=\"a\"/><itemref idref=\"b\"/></spine></package>");
        write(u"toc.ncx"_s,
              "<ncx><navMap><navPoint><navLabel><text>First</text></navLabel><content src=\"a.xhtml\"/></navPoint>"
              "<navPoint><navLabel><text>Second</text></navLabel><content src=\"b.xhtml\"/></navPoint></navMap></ncx>");
        write(u"a.xhtml"_s, "<html><body><h1>First</h1><p>Once upon a time.</p><p><img src=\"pic.png\"/></p></body></html>");
        write(u"b.xhtml"_s, "<html><body><h1>Second</h1><p>The end.</p></body></html>");
        QImage picture(40, 30, QImage::Format_RGB32);
        picture.fill(Qt::darkGreen);
        QVERIFY(picture.save(dir.filePath(u"book/pic.png"_s)));
        QProcess pack;
        pack.setWorkingDirectory(dir.filePath(u"book"_s));
        pack.start(bsdtar, {u"--format"_s, u"zip"_s, u"-cf"_s, dir.filePath(u"tiny.epub"_s), u"mimetype"_s, u"META-INF"_s, u"book.opf"_s, u"toc.ncx"_s,
                            u"a.xhtml"_s, u"b.xhtml"_s, u"pic.png"_s});
        QVERIFY(pack.waitForFinished(10000) && pack.exitCode() == 0);

        Viewer *v = Viewer::instance();
        v->setWindow(window);
        v->Open({dir.filePath(u"tiny.epub"_s)}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        // Wait for the book itself: the view before it may have been a TextView too.
        QTRY_COMPARE_WITH_TIMEOUT(v->formattedKind(), u"book"_s, 10000);
        QCOMPARE(v->property("view").toString(), u"TextView"_s);
        QVERIFY(v->showFormatted());
        QVERIFY(v->text().contains(u"Once upon a time."_s));
        QQuickItem *textView = nullptr;
        QTRY_VERIFY((textView = window->findChild<QQuickItem *>(u"textView"_s)));
        QVERIFY(textView->property("rendered").toBool());
        // The book's table of contents, at its chapters.
        QTRY_COMPARE(textView->property("contents").toList().size(), 2);
        const QVariantList contents = textView->property("contents").toList();
        QCOMPARE(contents.at(0).toMap().value(u"title"_s).toString(), u"First"_s);
        QVERIFY(contents.at(1).toMap().value(u"position"_s).toInt() > contents.at(0).toMap().value(u"position"_s).toInt());
        // Built whole: Qt's partial building lost the top after a table (Contents::drawWhole).
        QVERIFY(!window->findChild<QQuickItem *>(u"textArea"_s)->flags().testFlag(QQuickItem::ItemObservesViewport));
        // Never coloured as code (it once was, for a moment, during the switch).
        QVERIFY(!window->findChild<QQuickItem *>(u"textArea"_s)->findChild<QObject *>(u"highlighter"_s));
        // No formatted / source toggle for books.
        QQuickItem *toggle = findItem(window, u"barViewMode"_s);
        QVERIFY(!toggle || !toggle->isVisible());
        // The picture inside the book loads from its data address.
        QQuickItem *area = window->findChild<QQuickItem *>(u"textArea"_s);
        QTextDocument *doc = area->property("textDocument").value<QQuickTextDocument *>()->textDocument();
        QString image;
        for (QTextBlock block = doc->begin(); block.isValid() && image.isEmpty(); block = block.next()) {
            for (auto it = block.begin(); !it.atEnd(); ++it) {
                if (it.fragment().charFormat().isImageFormat()) {
                    image = it.fragment().charFormat().toImageFormat().name();
                }
            }
        }
        QVERIFY(image.startsWith(u"data:image/png;base64,"_s));
        QTRY_VERIFY(!doc->resource(QTextDocument::ImageResource, QUrl(image)).isNull());
        QVERIFY2(main.warnings().isEmpty(), qPrintable(main.warnings()));
        v->Close();
        v->setWindow(nullptr);
    }
};

QTEST_MAIN(TstQmlSmokeText)
#include "tst_qmlsmoke_text.moc"
