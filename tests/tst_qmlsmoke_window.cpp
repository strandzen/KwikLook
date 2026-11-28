// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>

// Smoke tests of the preview window, offscreen: opening the preview, its bar and settings, switching files, the index sheet, placement and archives.

#include "qmlsmoke.h"

class TstQmlSmokeWindow : public QObject
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
        Viewer::instance()->setWindow(main.window()); // the view loads only while shown
        Viewer::instance()->Open({file.fileName()}, 0, QString(), QString());
        QTRY_COMPARE(Viewer::instance()->view(), QStringLiteral("FallbackView"));
        QTRY_VERIFY(main.root()->findChild<QQuickItem *>(QStringLiteral("fallbackView")));
        // One file: no position in the corner.
        QVERIFY(!main.root()->findChild<QQuickItem *>(u"position"_s)->isVisible());
        QTest::qWait(200); // let the thumbnail request finish or fail
        // Not an AppImage: no install button.
        QVERIFY(!main.root()->findChild<QQuickItem *>(u"installAppImage"_s)->isVisible());
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
        Viewer::instance()->Close();
        Viewer::instance()->setWindow(nullptr);
    }

    void offersAppImageManagerForAppImages()
    {
        MainWindow main;
        QTemporaryDir dir;
        // An AppImage by content: ELF header with the AppImage mark (never run).
        QFile file(dir.filePath(u"Tool-x86_64.AppImage"_s));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QByteArray header("\x7f" "ELF\x02\x01\x01\x00" "AI\x02", 11);
        file.write(header + QByteArray(4000, '\0'));
        file.close();
        Viewer::instance()->setWindow(main.window()); // the view loads only while shown
        Viewer::instance()->Open({file.fileName()}, 0, QString(), QString());
        QTRY_COMPARE(Viewer::instance()->mimeType(), u"application/vnd.appimage"_s);
        QTRY_COMPARE(Viewer::instance()->view(), u"FallbackView"_s);
        QQuickItem *button = nullptr;
        QTRY_VERIFY((button = main.root()->findChild<QQuickItem *>(u"installAppImage"_s)));
        // Shown exactly when AppImage Manager is installed (not clicked: it would start it).
        const bool installed = !FileMenus::applicationIcon(u"io.github.strandzen.AppImageManager"_s).isEmpty();
        QCOMPARE(button->isVisible(), installed);
        QVERIFY(FileMenus::applicationIcon(u"io.github.strandzen.NoSuchApp"_s).isEmpty());
        QTest::qWait(200);
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
        Viewer::instance()->Close();
        Viewer::instance()->setWindow(nullptr);
    }

    // The shown file goes away: the view stays, the note shows over it.
    void deletedFileShowsANote()
    {
        MainWindow main;
        QTemporaryDir dir;
        const QString path = dir.filePath(u"notes.txt"_s);
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("some text\n");
        file.close();
        Viewer::instance()->setWindow(main.window()); // the view loads only while shown
        Viewer::instance()->Open({path}, 0, QString(), QString());
        QObject *root = main.root();
        QTRY_VERIFY(root->findChild<QQuickItem *>(u"textView"_s));
        QQuickItem *note = root->findChild<QQuickItem *>(u"goneMessage"_s);
        QVERIFY(note);
        QVERIFY(!note->isVisible());
        QVERIFY(QFile::remove(path));
        QTRY_VERIFY(note->isVisible());
        QVERIFY(root->findChild<QQuickItem *>(u"textView"_s)); // still shown
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
        Viewer::instance()->Close();
        Viewer::instance()->setWindow(nullptr);
    }

    // Position and zoom share one box in the corner: “7 / 30 | 90%” while both show;
    // a part leaves after its second, the box fades after the last.
    void cornerLabelSharesOneBox()
    {
        MainWindow main;
        QQuickItem *corner = main.root()->findChild<QQuickItem *>(u"cornerLabel"_s);
        QQuickItem *position = main.root()->findChild<QQuickItem *>(u"position"_s);
        QQuickItem *zoom = main.root()->findChild<QQuickItem *>(u"zoomLevel"_s);
        QVERIFY(corner && position && zoom);
        // Layouts only update in a window that draws: show one file (no position flash).
        QTemporaryFile file;
        QVERIFY(file.open());
        file.write("text");
        file.flush();
        Viewer *v = Viewer::instance();
        v->setWindow(main.window());
        auto cleanup = qScopeGuard([v] {
            v->Close();
            v->setWindow(nullptr);
        });
        v->Open({file.fileName()}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(main.window()));
        QVERIFY(!corner->isVisible());
        QMetaObject::invokeMethod(corner, "flashPosition");
        QTest::qWait(600);
        QMetaObject::invokeMethod(corner, "flashZoom");
        QVERIFY(corner->isVisible());
        QVERIFY(position->isVisible() && zoom->isVisible()); // side by side
        const qreal both = corner->width();
        QTRY_VERIFY_WITH_TIMEOUT(!position->isVisible(), 1500); // its second is up first
        QVERIFY(zoom->isVisible()); // the zoom part stays (its second runs 0.6 s longer)
        QCOMPARE(corner->opacity(), 1.0);
        QTRY_VERIFY(corner->width() < both); // laid out again in the next frame
        QTRY_VERIFY_WITH_TIMEOUT(!corner->isVisible(), 3000); // then the box fades
        QVERIFY(!zoom->isVisible()); // and starts empty next time
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
    }

    // What KwikLook's KWin script is told, and the window waiting for the first file's
    // details while the script is loaded (so it can take the picture's shape as it shows).
    void placementAnswersAndWaitsForTheFirstFile()
    {
        MainWindow main;
        QQuickWindow *window = main.window();
        QVERIFY(window);
        QTemporaryDir dir;
        const QString pic = dir.filePath(u"pic.png"_s);
        QImage img(800, 600, QImage::Format_RGB32);
        img.fill(Qt::darkGreen);
        QVERIFY(img.save(pic));
        const QString txt = dir.filePath(u"notes.txt"_s);
        QFile f(txt);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("text");
        f.close();
        Viewer *v = Viewer::instance();
        KwikLookSettings *settings = KwikLookSettings::self();
        auto cleanup = qScopeGuard([v, settings] {
            v->Close();
            v->setPlacementActive(false);
            v->setWindow(nullptr);
            settings->setWindowPosition(settings->defaultWindowPositionValue());
        });
        const auto answer = [v](const QString &caption) {
            return QJsonDocument::fromJson(v->Placement(caption).toUtf8()).object();
        };

        v->setInfoOpen(false); // remembered: another test may have left it open
        v->setWindow(window);
        v->setPlacementActive(true);
        v->Open({pic, txt}, 0, u"dolphin-1"_s, QString());
        QVERIFY(!window->isVisible()); // waits for the details…
        QTRY_VERIFY(window->isVisible());
        QCOMPARE(v->view(), u"ImageView"_s); // …which are there when it shows

        QJsonObject a = answer(window->title() + u" — KwikLook"_s);
        QVERIFY(a.value(u"preview"_s).toBool());
        QCOMPARE(a.value(u"mode"_s).toString(), u"screen"_s);
        const qreal dpr = window->devicePixelRatio();
        QCOMPARE(a.value(u"picture"_s).toObject().value(u"width"_s).toInt(), int(std::ceil(800 / dpr)));
        QCOMPARE(a.value(u"picture"_s).toObject().value(u"height"_s).toInt(), int(std::ceil(600 / dpr)));
        const QJsonObject chrome = a.value(u"chrome"_s).toObject();
        QCOMPARE(chrome.value(u"width"_s).toInt(), 0); // edge to edge, information panel closed
        QVERIFY(chrome.value(u"height"_s).toInt() > 0); // the bar
        QCOMPARE(a.value(u"minimum"_s).toObject().value(u"width"_s).toInt(), window->minimumWidth());
        QVERIFY(!answer(u"Settings — KwikLook"_s).value(u"preview"_s).toBool()); // the other window

        settings->setWindowPosition(u"Dolphin"_s);
        QCOMPARE(answer(window->title()).value(u"mode"_s).toString(), u"dolphin"_s);

        v->Next();
        QTRY_COMPARE(v->view(), u"TextView"_s);
        QVERIFY(answer(window->title()).value(u"picture"_s).isNull());

        v->Close();
        QVERIFY(!answer(window->title()).value(u"preview"_s).toBool());
        v->Open({txt}, 0, QString(), QString()); // `kwiklook <file>`: not from Dolphin
        QTRY_VERIFY(window->isVisible());
        QCOMPARE(answer(window->title()).value(u"mode"_s).toString(), u"screen"_s);
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
    }

    void switchesFilesInOneStep()
    {
        MainWindow main;
        QQuickWindow *window = main.window();
        QVERIFY(window);
        QTemporaryDir dir;
        QStringList files;
        for (const QString &name : {u"a.txt"_s, u"b.txt"_s}) {
            QFile f(dir.filePath(name));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("text of " + name.toUtf8() + "\n");
            files << f.fileName();
        }
        Viewer *v = Viewer::instance();
        v->setWindow(window);
        v->Open(files, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTRY_COMPARE(v->text(), u"text of a.txt\n"_s);
        QTRY_VERIFY(!v->switching());
        QSignalSpy aboutToSwitch(v, &Viewer::aboutToSwitch);
        v->Next();
        QVERIFY(v->switching());
        QCOMPARE(v->currentIndex(), 0); // still the first, with its text
        QCOMPARE(v->text(), u"text of a.txt\n"_s);
        QTRY_COMPARE(v->currentIndex(), 1);
        QCOMPARE(v->text(), u"text of b.txt\n"_s);
        QVERIFY(!v->switching());
        QCOMPARE(aboutToSwitch.count(), 1);
        QQuickItem *lastView = findItem(window, u"lastView"_s);
        QVERIFY(lastView);
        QTRY_COMPARE(lastView->opacity(), 0.0); // faded
        QVERIFY(!lastView->property("sourceItem").value<QQuickItem *>()); // picture freed
        QVERIFY(!findItem(window, u"switchingBusy"_s)->isVisible());
        v->Close();
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
        QTRY_COMPARE(v->currentIndex(), 2); // once its details are read
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

    void listsAnArchive()
    {
        if (!Sandbox::supported()) {
            QSKIP("this kernel's Landlock can't lock the archive helper in (archives are read in the sandbox)");
        }
        const QString bsdtar = QStandardPaths::findExecutable(u"bsdtar"_s);
        if (bsdtar.isEmpty()) {
            QSKIP("bsdtar is needed to make the test archive");
        }
        MainWindow main;
        QQuickWindow *window = main.window();
        QVERIFY(window);
        QTemporaryDir dir;
        QDir().mkpath(dir.filePath(u"tree/project/src"_s));
        for (const auto &[name, data] : {std::pair{u"project/readme.md"_s, QByteArray("# Project\n")}, std::pair{u"project/src/main.cpp"_s, QByteArray(2048, ' ')}}) {
            QFile f(dir.filePath(u"tree/"_s + name));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(data);
        }
        QProcess pack;
        pack.setWorkingDirectory(dir.filePath(u"tree"_s));
        pack.start(bsdtar, {u"--format"_s, u"zip"_s, u"-cf"_s, dir.filePath(u"project.zip"_s), u"project"_s});
        QVERIFY(pack.waitForFinished(10000) && pack.exitCode() == 0);
        Viewer *v = Viewer::instance();
        v->setWindow(window);
        v->Open({dir.filePath(u"project.zip"_s)}, 0, QString(), QString());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QQuickItem *view = nullptr;
        QTRY_VERIFY((view = window->findChild<QQuickItem *>(u"archiveView"_s)));
        QObject *model = view->findChild<QObject *>(u"archiveModel"_s);
        QVERIFY(model);
        QTRY_COMPARE_WITH_TIMEOUT(model->property("status").toString(), u"ready"_s, 10000);
        // 2 files, 2 folders (project, src), sizes; nothing locked.
        QQuickItem *summary = window->findChild<QQuickItem *>(u"archiveSummary"_s);
        QVERIFY(summary->property("text").toString().contains(u"2 files, 2 folders"_s));
        QVERIFY(!model->property("encrypted").toBool());
        // The one top folder opens by itself: its contents show, folders first.
        QQuickItem *tree = window->findChild<QQuickItem *>(u"archiveTree"_s);
        QTRY_COMPARE(tree->property("rows").toInt(), 3); // project, src, readme.md
        QStringList shown;
        QTRY_VERIFY([&] {
            shown.clear();
            for (QQuickItem *entry : itemsNamed(tree, u"archiveEntry"_s)) {
                if (entry->property("column").toInt() != 0) {
                    continue;
                }
                // KDE's delegate draws the model's display text in a label of its own.
                QList<QQuickItem *> todo{entry};
                while (!todo.isEmpty()) {
                    QQuickItem *item = todo.takeFirst();
                    if (item->inherits("QQuickText") && !item->property("text").toString().isEmpty()) {
                        shown << item->property("text").toString();
                        break;
                    }
                    todo << item->childItems();
                }
            }
            return shown.size() == 3;
        }());
        QVERIFY2(shown.contains(u"readme.md"_s) && shown.contains(u"src"_s), qPrintable(shown.join(u", "_s)));
        QVERIFY(window->findChild<QQuickItem *>(u"archiveMessage"_s)->property("text").toString().isEmpty());
        v->Close();
        v->setWindow(nullptr);
        QVERIFY2(main.errors.isEmpty(), qPrintable(main.warnings()));
    }

    void barFollowsTheSettings()
    {
        KwikLookSettings *settings = KwikLookSettings::self();
        settings->setBarLeft({u"Close"_s});
        // "CropRotate": an item that no longer exists, from an older version's settings.
        settings->setBarRight({u"Spacer"_s, u"CopyPath"_s, u"CropRotate"_s, u"Open"_s});
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
        QVERIFY(!findItem(window, u"barCropRotate"_s)); // left out, no empty button
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

    void shareOffersLocalSend()
    {
        const bool installed = KService::serviceByDesktopName(u"localsend"_s) || KService::serviceByDesktopName(u"org.localsend.localsend_app"_s)
            || KService::serviceByDesktopName(u"localsend_app"_s);
        if (!installed) {
            QSKIP("LocalSend isn't installed");
        }
        const QString txt = m_keptDir.filePath(u"share.txt"_s);
        QFile f(txt);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        FileMenus menus;
        menus.setProperty("url", QUrl::fromLocalFile(txt));
        menus.setProperty("mimeType", u"text/plain"_s);
        QMenu *menu = menus.shareMenu();
        QVERIFY(menu);
        QVERIFY(menu->findChild<QAction *>(u"shareLocalSend"_s));
        // Once only, also when the menu is filled again.
        menu = menus.shareMenu();
        QCOMPARE(menu->findChildren<QAction *>(u"shareLocalSend"_s).size(), 1);
        // Not for a file that isn't on this computer.
        menus.setProperty("url", QUrl(u"https://example.org/share.txt"_s));
        menu = menus.shareMenu();
        QVERIFY(!menu->findChild<QAction *>(u"shareLocalSend"_s));
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

QTEST_MAIN(TstQmlSmokeWindow)
#include "tst_qmlsmoke_window.moc"
