// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "gui/autostart.h"
#include "gui/filemenus.h"
#include "gui/offlinenetwork.h"
#include "gui/thumbnailprovider.h"
#include "gui/videoeditor.h"
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
#include <KSharedConfig>
#include <QDirIterator>
#include <KService>
#include <QMenu>
#include <QRawFont>
#include <QFontDatabase>
#include <QSignalSpy>
#include <QFileInfo>
#include <QProcess>
#include <QScopeGuard>
#include <QTcpServer>

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
        engine.setNetworkAccessManagerFactory(OfflineNetworkFactory::instance());
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

// Items a Repeater or Loader made aren't QObject children of the window: walk the items.
static QQuickItem *findItem(QQuickWindow *window, const QString &name)
{
    QList<QQuickItem *> todo{window->contentItem()};
    while (!todo.isEmpty()) {
        QQuickItem *item = todo.takeFirst();
        if (item->objectName() == name) {
            return item;
        }
        todo << item->childItems();
    }
    return nullptr;
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
        QVERIFY(!v->fontFamily().isEmpty());
        QQuickItem *sample = findItem(window, u"fontSample"_s); // a Repeater row
        QVERIFY(sample);
        QCOMPARE(sample->property("font").value<QFont>().family(), v->fontFamily());
        // A font from /usr/share/fonts is installed: no button, and nothing copied.
        QVERIFY(v->fontInstalled());
        QVERIFY(findItem(window, u"fontInstalled"_s)->isVisible());
        QVERIFY(!findItem(window, u"installFont"_s)->isVisible());
        v->installFont();
        QVERIFY(!QFileInfo::exists(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + u"/fonts"_s));
        // One size: 36 px, − / + through the usual sizes, Ctrl+0 back.
        QCOMPARE(sample->property("font").value<QFont>().pixelSize(), 36);
        QMetaObject::invokeMethod(findItem(window, u"fontLarger"_s), "clicked");
        QCOMPARE(v->property("fontSampleSize").toInt(), 48);
        QCOMPARE(sample->property("font").value<QFont>().pixelSize(), 48);
        QMetaObject::invokeMethod(findItem(window, u"fontSmaller"_s), "clicked");
        QMetaObject::invokeMethod(findItem(window, u"fontSmaller"_s), "clicked");
        QCOMPARE(v->property("fontSampleSize").toInt(), 28);
        QTest::keyClick(window, Qt::Key_0, Qt::ControlModifier);
        QCOMPARE(v->property("fontSampleSize").toInt(), 36);
        QTest::keyClick(window, Qt::Key_Plus, Qt::ControlModifier);
        QCOMPARE(v->property("fontSampleSize").toInt(), 45); // Ctrl+plus: × 1.25
        // Typing: the samples show the text; Enter and ← → don't act on the preview.
        auto *field = window->findChild<QQuickItem *>(u"fontSampleField"_s);
        QVERIFY(field);
        field->forceActiveFocus();
        for (const char c : {'H', 'e', 'l', 'l', 'o'}) {
            QTest::keyClick(window, c); // no keyClicks() for QWindow
        }
        QCOMPARE(v->property("fontSampleText").toString(), u"Hello"_s);
        QCOMPARE(sample->property("text").toString(), u"Hello"_s);
        QTest::keyClick(window, Qt::Key_Right);
        QTest::keyClick(window, Qt::Key_Return);
        QCOMPARE(v->currentIndex(), 0);
        QVERIFY(window->isVisible());
        // The text stays for the next file; a broken font shows the fallback.
        v->Next();
        QTRY_VERIFY(window->findChild<QQuickItem *>(u"fallbackView"_s));
        QVERIFY(v->fontFamily().isEmpty());
        QCOMPARE(v->property("fontSampleText").toString(), u"Hello"_s);
        QCOMPARE(v->property("fontSampleSize").toInt(), 45); // kept too
        // Closing forgets them.
        v->Close();
        QCOMPARE(v->property("fontSampleText").toString(), QString());
        QCOMPARE(v->property("fontSampleSize").toInt(), 36);
        v->setWindow(nullptr);
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
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
        QMetaObject::invokeMethod(pdfView, "findNext");
        QMetaObject::invokeMethod(pdfView, "findNext");
        QCOMPARE(count->property("text").toString(), u"3 of 3"_s);
        QObject *search = nullptr;
        for (QObject *o : pdfView->findChildren<QObject *>()) {
            if (o->inherits("QPdfSearchModel")) {
                search = o;
            }
        }
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
        QVERIFY(!v->fontInstalled());
        QQuickItem *button = findItem(window, u"installFont"_s);
        QVERIFY(button && button->isVisible());
        QSignalSpy finished(v, &Viewer::fontInstallFinished);
        QMetaObject::invokeMethod(button, "clicked");
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(finished.first().first().toBool(), true);
        const QString copy = fonts + u'/' + QFileInfo(fontPath).fileName();
        QVERIFY(QFileInfo::exists(copy));
        QVERIFY(v->fontInstalled());
        QTRY_VERIFY(findItem(window, u"fontInstalled"_s)->isVisible());
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

    void indexSheetPicksAFile()
    {
        MainWindow main;
        QQuickWindow *window = main.window();
        QVERIFY(window);
        QTemporaryDir dir;
        QStringList files;
        for (const QString &name : {u"a.txt"_s, u"b.txt"_s, u"c.txt"_s}) {
            QFile f(dir.filePath(name));
            QVERIFY(f.open(QIODevice::WriteOnly));
            files << f.fileName();
        }
        Viewer *v = Viewer::instance();
        v->setWindow(window);
        v->Open(files, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTRY_VERIFY(window->findChild<QQuickItem *>(u"textArea"_s));
        QVERIFY(findItem(window, u"barIndexSheet"_s)->isVisible());
        // Ctrl+Enter: the sheet, on the current file, with the keyboard.
        QTest::keyClick(window, Qt::Key_Return, Qt::ControlModifier);
        QVERIFY(v->property("indexOpen").toBool());
        QQuickItem *grid = nullptr;
        QTRY_VERIFY((grid = findItem(window, u"indexGrid"_s)) && grid->hasActiveFocus());
        QCOMPARE(grid->property("currentIndex").toInt(), 0);
        QCOMPARE(v->fileAt(2).value(u"name"_s).toString(), u"c.txt"_s);
        // Arrows move in the grid, not between files; Enter shows the chosen one.
        QTest::keyClick(window, Qt::Key_Right);
        QTest::keyClick(window, Qt::Key_Right);
        QCOMPARE(grid->property("currentIndex").toInt(), 2);
        QCOMPARE(v->currentIndex(), 0);
        QTest::keyClick(window, Qt::Key_Return);
        QCOMPARE(v->currentIndex(), 2);
        QVERIFY(!v->property("indexOpen").toBool());
        QVERIFY(window->isVisible());
        // Esc closes the sheet first, then the preview.
        QTest::keyClick(window, Qt::Key_Return, Qt::ControlModifier);
        QTRY_VERIFY(findItem(window, u"indexGrid"_s));
        QTest::keyClick(window, Qt::Key_Escape);
        QVERIFY(!v->property("indexOpen").toBool());
        QVERIFY(window->isVisible());
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!window->isVisible());
        // One file: no sheet.
        v->Open({files.first()}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTest::keyClick(window, Qt::Key_Return, Qt::ControlModifier);
        QVERIFY(!v->property("indexOpen").toBool());
        QVERIFY(!findItem(window, u"barIndexSheet"_s)->isVisible());
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

    void barFollowsTheSettings()
    {
        KwikLookSettings *settings = KwikLookSettings::self();
        settings->setBarLeft({u"Close"_s});
        settings->setBarRight({u"Spacer"_s, u"CopyPath"_s, u"Open"_s});
        const auto restore = qScopeGuard([settings] {
            settings->setBarLeft(settings->defaultBarLeftValue());
            settings->setBarRight(settings->defaultBarRightValue());
        });
        MainWindow main;
        QQuickWindow *window = main.window();
        QVERIFY(window);
        Viewer *v = Viewer::instance();
        v->setWindow(window);
        QFile f(m_keptDir.filePath(u"bar.txt"_s));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        v->Open({f.fileName()}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTRY_VERIFY(findItem(window, u"barCopyPath"_s));
        QVERIFY(findItem(window, u"barOpen"_s));
        QVERIFY(!findItem(window, u"barPrevious"_s));
        QQuickItem *close = findItem(window, u"barClose"_s);
        QVERIFY(close);
        QQuickItem *bar = window->findChild<QQuickItem *>(u"titleBar"_s);
        QVERIFY(close->mapToItem(bar, QPointF()).x() < bar->width() / 2); // on the left now
        // Changes show at once (the settings window applies in the same process).
        settings->setBarLeft(settings->defaultBarLeftValue());
        settings->setBarRight(settings->defaultBarRightValue());
        QTRY_VERIFY(findItem(window, u"barPrevious"_s));
        QTRY_VERIFY(!findItem(window, u"barCopyPath"_s));
        // A button does its job.
        QQuickItem *info = findItem(window, u"barInformation"_s);
        QVERIFY(info);
        const bool open = v->infoOpen();
        QMetaObject::invokeMethod(info, "clicked");
        QCOMPARE(v->infoOpen(), !open);
        v->setInfoOpen(open);
        v->Close();
        v->setWindow(nullptr);
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
    }

    void settingsBarEditorMovesItems()
    {
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
        window->resize(700, 900);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window));
        KwikLookSettings *settings = KwikLookSettings::self();
        QObject *editor = window->findChild<QObject *>(u"barEditor"_s);
        QVERIFY(editor);
        const auto items = [editor](const char *name) {
            return editor->property(name).toStringList();
        };
        const auto move = [editor](const QString &from, int fromIndex, const QString &to, int toIndex) {
            QMetaObject::invokeMethod(editor, "move", Q_ARG(QVariant, from), Q_ARG(QVariant, fromIndex), Q_ARG(QVariant, to), Q_ARG(QVariant, toIndex));
        };
        QCOMPARE(items("leftItems"), settings->barLeft());
        QVERIFY(!items("available").contains(u"Previous"_s));
        QCOMPARE(items("available").last(), u"Spacer"_s);
        // Out of the bar, along it, into it; the flexible space is copied, not moved.
        move(u"right"_s, 4, u"available"_s, 0); // Close out
        QCOMPARE(items("rightItems"), QStringList({u"ViewMode"_s, u"Information"_s, u"Share"_s, u"OpenWith"_s}));
        QVERIFY(items("available").contains(u"Close"_s));
        move(u"left"_s, 0, u"left"_s, 2); // Previous after Next
        QCOMPARE(items("leftItems"), QStringList({u"Next"_s, u"Previous"_s, u"IndexSheet"_s, u"Pages"_s}));
        const int spacer = items("available").indexOf(u"Spacer"_s);
        move(u"available"_s, spacer, u"right"_s, 0);
        move(u"available"_s, items("available").indexOf(u"Spacer"_s), u"right"_s, 0);
        QCOMPARE(items("rightItems").count(u"Spacer"_s), 2);
        QVERIFY(items("available").contains(u"Spacer"_s));
        QCOMPARE(window->property("changed").toBool(), true);
        QCOMPARE(settings->barRight().size(), 5); // not until Apply

        // A real drag: Copy Path from below into the bar's left side (once the rows have
        // been laid out again after the moves above).
        QTest::qWait(200);
        QQuickItem *chip = findItem(window, u"chipavailableCopyPath"_s);
        QQuickItem *zone = window->findChild<QQuickItem *>(u"zoneLeft"_s);
        QVERIFY(chip && zone);
        const QPoint from = chip->mapToScene(QPointF(chip->width() / 2, chip->height() / 2)).toPoint();
        const QPoint to = zone->mapToScene(QPointF(4, zone->height() / 2)).toPoint();
        QTest::mousePress(window, Qt::LeftButton, {}, from);
        for (int i = 1; i <= 10; ++i) {
            QTest::mouseMove(window, from + (to - from) * i / 10);
        }
        QTest::mouseRelease(window, Qt::LeftButton, {}, to);
        QTRY_COMPARE(items("leftItems"), QStringList({u"CopyPath"_s, u"Next"_s, u"Previous"_s, u"IndexSheet"_s, u"Pages"_s}));

        // Apply saves; Reset brings the defaults back (until Apply).
        QVariant ok;
        QVERIFY(QMetaObject::invokeMethod(window, "apply", Q_RETURN_ARG(QVariant, ok)));
        QCOMPARE(settings->barLeft(), QStringList({u"CopyPath"_s, u"Next"_s, u"Previous"_s, u"IndexSheet"_s, u"Pages"_s}));
        QQuickItem *reset = window->findChild<QQuickItem *>(u"barReset"_s);
        QVERIFY(reset);
        QMetaObject::invokeMethod(reset, "clicked");
        QCOMPARE(items("leftItems"), settings->defaultBarLeftValue());
        QCOMPARE(items("rightItems"), settings->defaultBarRightValue());
        QVERIFY(QMetaObject::invokeMethod(window, "apply", Q_RETURN_ARG(QVariant, ok)));
        QCOMPARE(settings->barRight(), settings->defaultBarRightValue());

        for (const QQmlError &e : std::as_const(errors)) {
            qWarning().noquote() << e.toString();
        }
        QVERIFY(errors.isEmpty());
    }

    void openWithOffersSpectacleForPictures()
    {
        if (!KService::serviceByDesktopName(u"org.kde.spectacle"_s)) {
            QSKIP("Spectacle isn't installed");
        }
        QImage picture(4, 4, QImage::Format_RGB32);
        picture.fill(Qt::red);
        const QString png = m_keptDir.filePath(u"menu.png"_s);
        QVERIFY(picture.save(png));
        FileMenus menus;
        menus.setProperty("url", QUrl::fromLocalFile(png));
        menus.setProperty("mimeType", u"image/png"_s);
        std::unique_ptr<QMenu> menu(menus.openWithMenu());
        QVERIFY(menu);
        QVERIFY(menu->findChild<QAction *>(u"openWithSpectacle"_s));
        // Not for other files.
        menus.setProperty("url", QUrl::fromLocalFile(m_keptDir.filePath(u"menu.txt"_s)));
        menus.setProperty("mimeType", u"text/plain"_s);
        menu.reset(menus.openWithMenu());
        QVERIFY(!menu->findChild<QAction *>(u"openWithSpectacle"_s));
    }

    void videoTrimAndRotate()
    {
        if (!VideoEditor::available()) {
            QSKIP("ffmpeg, ffprobe or the sandbox isn't available");
        }
        QTemporaryDir dir;
        const QString clip = dir.filePath(u"clip.mp4"_s);
        QProcess ffmpeg;
        ffmpeg.start(u"ffmpeg"_s, {u"-loglevel"_s, u"error"_s, u"-y"_s, u"-f"_s, u"lavfi"_s, u"-i"_s, u"testsrc=duration=4:size=160x120:rate=25"_s,
                                   u"-g"_s, u"25"_s, u"-pix_fmt"_s, u"yuv420p"_s, clip});
        QVERIFY(ffmpeg.waitForFinished(30000) && ffmpeg.exitCode() == 0);
        KwikLookSettings::self()->setAutoplayMedia(false);
        KwikLookSettings::self()->setStartMuted(true);
        MainWindow main;
        QQuickWindow *window = main.window();
        QVERIFY(window);
        Viewer *v = Viewer::instance();
        v->setWindow(window);
        v->Open({clip}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QQuickItem *media = nullptr;
        QTRY_VERIFY((media = window->findChild<QQuickItem *>(u"mediaView"_s)));
        QQuickItem *edit = window->findChild<QQuickItem *>(u"editButton"_s);
        QTRY_VERIFY(edit->isVisible()); // once the player knows it's a video
        // Playing, then Edit: stopped at the start; only Play plays (inside the part).
        QObject *player = window->findChild<QObject *>(u"mediaPlayer"_s);
        QMetaObject::invokeMethod(window->findChild<QQuickItem *>(u"playButton"_s), "clicked");
        QTRY_COMPARE(player->property("playbackState").toInt(), 1); // PlayingState
        QMetaObject::invokeMethod(edit, "clicked");
        QVERIFY(media->property("editing").toBool());
        QTRY_COMPARE(player->property("playbackState").toInt(), 2); // PausedState
        QTRY_VERIFY(player->property("position").toLongLong() < 100);
        QMetaObject::invokeMethod(media, "toggleFullScreen");
        QCOMPARE(window->visibility(), QWindow::Windowed); // not while editing
        // Edit: the bar, keyframes read; nothing to save until something changes.
        QQuickItem *save = window->findChild<QQuickItem *>(u"saveCopy"_s);
        QTRY_VERIFY(save->isVisible() && !save->isEnabled());
        auto *editor = media->findChild<VideoEditor *>();
        QVERIFY(editor);
        QTRY_VERIFY(!editor->probing());
        QCOMPARE(editor->keyframes().size(), 4);
        // Rotate left: a quarter turn counterclockwise, then Save Copy.
        QMetaObject::invokeMethod(window->findChild<QQuickItem *>(u"rotateLeft"_s), "clicked");
        QCOMPARE(media->property("turn").toInt(), 90);
        QTRY_VERIFY(save->isEnabled());
        QSignalSpy saved(editor, &VideoEditor::saved);
        QMetaObject::invokeMethod(save, "clicked");
        QTRY_VERIFY_WITH_TIMEOUT(saved.size() == 1, 30000);
        QVERIFY(QFile::exists(dir.filePath(u"clip (edited).mp4"_s)));
        QTRY_VERIFY(!media->property("editing").toBool()); // done: the bar closes
        // A start between keyframes: an exact cut.
        QMetaObject::invokeMethod(edit, "clicked");
        QTRY_VERIFY(!editor->probing());
        QQuickItem *range = window->findChild<QQuickItem *>(u"trimRange"_s);
        QObject *first = range->property("first").value<QObject *>();
        QQuickItem *kind = window->findChild<QQuickItem *>(u"cutKind"_s);
        QCOMPARE(kind->property("text").toString(), u"Quick"_s);
        first->setProperty("value", 1500); // ms; keyframes every second
        QCOMPARE(kind->property("text").toString(), u"Exact, slower"_s);
        saved.clear();
        QMetaObject::invokeMethod(save, "clicked");
        QTRY_VERIFY_WITH_TIMEOUT(saved.size() == 1, 60000);
        QCOMPARE(QDir(dir.path()).entryList({u"clip (edited)*"_s}).size(), 2); // a second copy, the first kept
        v->Close();
        v->setWindow(nullptr);
        KwikLookSettings::self()->setAutoplayMedia(true);
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
