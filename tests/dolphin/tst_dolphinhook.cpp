// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 KDEGlance contributors
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
// Based on KDEGlance's Dolphin integration test (https://github.com/AlexGraveler/KDEGlance).

// The Dolphin hook inside a stand-in Dolphin: this process plays Dolphin (mock
// view classes, applicationName "dolphin"), loads the real hook plugin, and
// plays the viewer too (a FakeViewer on the bus that records the calls and
// sends the viewer's signals). Runs on a private session bus (run-with-bus.sh).

#include "dbusnames.h"
#include "hookguard.h"
#include "mockdolphin.h"

#include <KConfig>
#include <KConfigGroup>
#include <KOverlayIconPlugin>
#include <KSharedConfig>

#include <QApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusMessage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPluginLoader>
#include <QTemporaryDir>
#include <QTest>
#include <QTextEdit>

#include <sys/stat.h>

using namespace Qt::StringLiterals;

// Records what the hook asks the viewer to do.
class FakeViewer : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.strandzen.KwikLook.Viewer")
public:
    struct Call {
        QString method;
        QStringList urls;
        int index = -1;
        QString session;
    };
    QList<Call> calls;
    bool failOpen = false; // answer Open with an error, like a viewer that couldn't start

    // Its own connection, like a separate viewer process.
    static QDBusConnection bus()
    {
        return QDBusConnection::connectToBus(QDBusConnection::SessionBus, u"fake-viewer"_s);
    }

    void emitSignal(const QString &name, const QVariantList &args)
    {
        QDBusMessage m = QDBusMessage::createSignal(QLatin1String(KwikLookDBus::ViewerPath), QLatin1String(KwikLookDBus::ViewerInterface), name);
        m.setArguments(args);
        bus().send(m);
    }

public Q_SLOTS:
    Q_SCRIPTABLE void Open(const QStringList &urls, int index, const QString &sessionId, const QString &activationToken)
    {
        Q_UNUSED(activationToken)
        calls.append({u"Open"_s, urls, index, sessionId});
        if (failOpen) {
            sendErrorReply(QDBusError::Failed, u"stand-in failure"_s);
        }
    }
    Q_SCRIPTABLE void Close()
    {
        calls.append({u"Close"_s, {}, -1, {}});
    }
};

class TstDolphinHook : public QObject
{
    Q_OBJECT
private:
    QTemporaryDir m_dir;
    std::unique_ptr<DolphinMainWindow> m_window;
    DolphinView *m_view = nullptr;
    QObject *m_bridge = nullptr;
    FakeViewer m_viewer;

    QUrl url(const QString &name) const
    {
        return QUrl::fromLocalFile(m_dir.filePath(name));
    }
    KFileItem fileItem(const QString &name, bool dir = false) const
    {
        return KFileItem(url(name), QString(), dir ? S_IFDIR : S_IFREG);
    }
    QJsonObject bridgeState()
    {
        QString state;
        QMetaObject::invokeMethod(m_bridge, "State", Qt::DirectConnection, Q_RETURN_ARG(QString, state));
        return QJsonDocument::fromJson(state.toUtf8()).object();
    }
    // The hook's guard record as on disk (a fresh KConfig: the hook's shared one
    // lives in this process too).
    QString guardEntry(const char *key)
    {
        return KConfig(HookGuard::defaultFile(), KConfig::SimpleConfig).group(u"DolphinHook"_s).readEntry(key, QString());
    }
    static QString testCase()
    {
        return qEnvironmentVariable("KWIKLOOK_TEST_HOOK_CASE");
    }
    QString session()
    {
        return bridgeState().value(u"session"_s).toString();
    }
    void focusView(DolphinView *view)
    {
        m_window->activateWindow();
        view->container()->setFocus();
        QTRY_COMPARE(QApplication::focusWidget(), view->container());
    }
    void pressSpace(DolphinView *view)
    {
        focusView(view);
        QTest::keyClick(view->container(), Qt::Key_Space);
    }
    // The next call the hook sends to the viewer.
    FakeViewer::Call nextCall()
    {
        const qsizetype before = m_viewer.calls.size();
        if (!QTest::qWaitFor([&] {
                return m_viewer.calls.size() > before;
            }, 3000)) {
            return {};
        }
        return m_viewer.calls.last();
    }
    void closeFromViewer()
    {
        m_viewer.emitSignal(u"Closed"_s, {session()});
        QTRY_VERIFY(session().isEmpty());
    }

private Q_SLOTS:
    void initTestCase()
    {
        // The hook writes its guard record to the state folder: never the user's.
        QVERIFY2(qEnvironmentVariableIsSet("XDG_STATE_HOME"), "run through run-with-bus.sh (ctest)");
        // Must look like Dolphin before the hook is loaded.
        QCoreApplication::setApplicationName(u"dolphin"_s);
        if (testCase() == u"crashed"_s) {
            // Left by a Dolphin that died inside the hook's first check.
            KConfig state(HookGuard::defaultFile(), KConfig::SimpleConfig);
            KConfigGroup group = state.group(u"DolphinHook"_s);
            group.writeEntry("Versions", HookGuard::versionsKey(QCoreApplication::applicationVersion()));
            group.writeEntry("Checking", u"read"_s);
            QVERIFY(state.sync());
        }
        auto bus = FakeViewer::bus();
        QVERIFY(bus.isConnected());
        QVERIFY(bus.registerObject(QLatin1String(KwikLookDBus::ViewerPath), &m_viewer, QDBusConnection::ExportScriptableSlots));
        QVERIFY(bus.registerService(QLatin1String(KwikLookDBus::Service)));

        QPluginLoader loader(QStringLiteral(KWIKLOOK_HOOK_PLUGIN));
        QObject *hook = loader.instance();
        QVERIFY2(hook, qPrintable(loader.errorString()));
        QVERIFY(qobject_cast<KOverlayIconPlugin *>(hook));
        const auto children = qApp->children();
        for (QObject *c : children) {
            if (c->inherits("Bridge")) {
                m_bridge = c;
            }
        }
        QVERIFY(m_bridge && m_bridge->inherits("Bridge"));

        m_window = std::make_unique<DolphinMainWindow>(2);
        m_view = m_window->view(0);
        // Dolphin's order (folders first, then descending by name here): the
        // preview must follow it, not the alphabet.
        const KFileItemList items{fileItem(u"folder"_s, true), fileItem(u"z1.png"_s), fileItem(u"y2.png"_s), fileItem(u"x3.png"_s), fileItem(u"w4.png"_s)};
        m_view->model()->setItems(items);
        m_window->view(1)->model()->setItems(items);
        m_window->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_window.get()));
    }

    void hookFoundDolphinInternals()
    {
        const QJsonObject s = bridgeState();
        QVERIFY(s.value(u"modelAccess"_s).toBool());
        QVERIFY(s.value(u"selectionAccess"_s).toBool());
        QCOMPARE(s.value(u"check"_s).toString(), u"pending"_s); // until the first Space
    }

    void spaceOpensSelectedFileInDolphinOrder()
    {
        m_view->selectOnly(2); // y2.png
        m_view->container()->keysReceived.clear();
        pressSpace(m_view);
        const FakeViewer::Call call = nextCall();
        QCOMPARE(call.method, u"Open"_s);
        QCOMPARE(call.urls,
                 (QStringList{url(u"z1.png"_s).toString(), url(u"y2.png"_s).toString(), url(u"x3.png"_s).toString(), url(u"w4.png"_s).toString()}));
        QCOMPARE(call.index, 1);
        QVERIFY(!call.session.isEmpty());
        QCOMPARE(session(), call.session);
        QVERIFY(!m_view->container()->keysReceived.contains(Qt::Key_Space)); // Dolphin never saw it
    }

    void viewerNavigationMovesDolphinsSelection()
    {
        KItemListSelectionManager *sel = m_view->container()->selectionManager();
        m_viewer.emitSignal(u"CurrentChanged"_s, {session(), url(u"x3.png"_s).toString(), 2});
        QTRY_VERIFY(sel->currentItem() == 3 && sel->selection() == QSet<int>{3});
        QCOMPARE(sel->anchor(), 3); // Shift+arrow in Dolphin extends from here
        QCOMPARE(m_view->container()->listView()->lastScrolled, 3);
        // The other (split) view is not touched.
        QCOMPARE(m_window->view(1)->container()->selectionManager()->changes, 0);
        // Another session's signals are ignored.
        m_viewer.emitSignal(u"CurrentChanged"_s, {u"someone-else"_s, url(u"w4.png"_s).toString(), 3});
        QTest::qWait(200);
        QCOMPARE(sel->currentItem(), 3);
    }

    void selfCheckPassesAndIsRemembered()
    {
        // After the first Space and the first selection change above.
        QCOMPARE(bridgeState().value(u"check"_s).toString(), u"ok"_s);
        QCOMPARE(guardEntry("Passed"), u"read,select"_s);
        QVERIFY(guardEntry("Checking").isEmpty());
        QVERIFY(guardEntry("Broken").isEmpty());
    }

    void closingGivesFocusBack()
    {
        m_window->view(1)->container()->setFocus();
        closeFromViewer();
        QTRY_VERIFY(m_view->container()->hasFocus());
        QCOMPARE(m_view->container()->selectionManager()->selection(), QSet<int>{3});
    }

    void spaceAgainClosesWhenDolphinKeptFocus()
    {
        pressSpace(m_view);
        QCOMPARE(nextCall().method, u"Open"_s);
        pressSpace(m_view);
        QCOMPARE(nextCall().method, u"Close"_s);
        closeFromViewer();
    }

    void previewFollowsDolphinsSelection()
    {
        m_view->selectOnly(1);
        pressSpace(m_view);
        QCOMPARE(nextCall().index, 0);
        m_view->selectOnly(3); // x3.png: Dolphin kept the focus, the user moved on
        const FakeViewer::Call call = nextCall();
        QCOMPARE(call.method, u"Open"_s);
        QCOMPARE(call.index, 2);
        QCOMPARE(call.session, session());
        // Our own selection changes cause no call back (no ping-pong).
        const qsizetype before = m_viewer.calls.size();
        m_viewer.emitSignal(u"CurrentChanged"_s, {session(), url(u"w4.png"_s).toString(), 3});
        QTRY_COMPARE(m_view->container()->selectionManager()->currentItem(), 4);
        QTest::qWait(200);
        QCOMPARE(m_viewer.calls.size(), before);
        closeFromViewer();
    }

    void severalSelectedFilesAreTheList()
    {
        // y2.png and w4.png selected (and the folder: folders never count), w4 current.
        KItemListSelectionManager *sel = m_view->container()->selectionManager();
        m_view->selectOnly(4);
        sel->setSelected(2);
        sel->setSelected(0);
        pressSpace(m_view);
        const FakeViewer::Call call = nextCall();
        QCOMPARE(call.method, u"Open"_s);
        QCOMPARE(call.urls, (QStringList{url(u"y2.png"_s).toString(), url(u"w4.png"_s).toString()}));
        QCOMPARE(call.index, 1);
        // Going through them leaves Dolphin's selection as it was.
        m_viewer.emitSignal(u"CurrentChanged"_s, {session(), url(u"y2.png"_s).toString(), 0});
        QTest::qWait(200);
        QCOMPARE(sel->selection(), (QSet<int>{0, 2, 4}));
        closeFromViewer();
        // One file selected again: the whole folder, as before.
        m_view->selectOnly(2);
        pressSpace(m_view);
        QCOMPARE(nextCall().urls.size(), 4);
        closeFromViewer();
    }

    void folderKeepsDolphinBehaviour()
    {
        m_view->selectOnly(0);
        m_view->container()->keysReceived.clear();
        const qsizetype before = m_viewer.calls.size();
        pressSpace(m_view);
        QTest::qWait(200);
        QVERIFY(m_view->container()->keysReceived.contains(Qt::Key_Space));
        QCOMPARE(m_viewer.calls.size(), before);
    }

    void typeAheadAndSelectionModeKeepSpace()
    {
        m_view->selectOnly(1);
        KItemListController *c = m_view->container()->controller();
        for (bool *flag : {&c->typeAheadActive, &c->selectionModeActive}) {
            *flag = true;
            m_view->container()->keysReceived.clear();
            const qsizetype before = m_viewer.calls.size();
            pressSpace(m_view);
            QTest::qWait(200);
            QVERIFY(m_view->container()->keysReceived.contains(Qt::Key_Space));
            QCOMPARE(m_viewer.calls.size(), before);
            *flag = false;
        }
    }

    void textInputKeepsSpace()
    {
        auto *editor = new QTextEdit(m_view->container()->viewport()); // like the inline rename editor
        editor->show();
        editor->setFocus();
        QTRY_COMPARE(QApplication::focusWidget(), editor);
        const qsizetype before = m_viewer.calls.size();
        QTest::keyClick(editor, Qt::Key_Space);
        QCOMPARE(editor->toPlainText(), u" "_s);
        QTest::qWait(200);
        QCOMPARE(m_viewer.calls.size(), before);
        delete editor;
    }

    void openKeyFollowsTheSettings()
    {
        // As the settings window saves it (XDG_CONFIG_HOME is a scratch folder here).
        const auto setOpenKey = [this](const QString &key) {
            KSharedConfig::Ptr config = KSharedConfig::openConfig(u"kwiklookrc"_s, KConfig::SimpleConfig);
            config->group(u"General"_s).writeEntry("OpenKey", key, KConfig::Notify);
            QVERIFY(config->sync());
            QTRY_COMPARE(bridgeState().value(u"openKey"_s).toString(), key);
        };
        setOpenKey(u"Shift+Space"_s);
        m_view->selectOnly(1);
        m_view->container()->keysReceived.clear();
        const qsizetype before = m_viewer.calls.size();
        pressSpace(m_view);
        QTest::qWait(200);
        QCOMPARE(m_viewer.calls.size(), before); // plain Space is Dolphin's again
        QVERIFY(m_view->container()->keysReceived.contains(Qt::Key_Space));
        QTest::keyClick(m_view->container(), Qt::Key_Space, Qt::ShiftModifier);
        QCOMPARE(nextCall().method, u"Open"_s);
        closeFromViewer();

        setOpenKey(u"Space"_s);
        pressSpace(m_view);
        QCOMPARE(nextCall().method, u"Open"_s);
        closeFromViewer();
    }

    void viewerGoneEndsTheSession()
    {
        // The viewer crashes with a preview open: no Closed arrives.
        m_view->selectOnly(1);
        pressSpace(m_view);
        QCOMPARE(nextCall().method, u"Open"_s);
        auto bus = FakeViewer::bus();
        QVERIFY(bus.unregisterService(QLatin1String(KwikLookDBus::Service)));
        QTRY_VERIFY(session().isEmpty());
        QVERIFY(bus.registerService(QLatin1String(KwikLookDBus::Service)));
        pressSpace(m_view);
        QCOMPARE(nextCall().method, u"Open"_s); // not Close
        closeFromViewer();
    }

    void failedOpenEndsTheSession()
    {
        // Open fails: no preview, so the next Space opens again instead of
        // sending Close (which used to leave Space dead in that view).
        m_view->selectOnly(1);
        m_viewer.failOpen = true;
        pressSpace(m_view);
        QCOMPARE(nextCall().method, u"Open"_s);
        QTRY_VERIFY(session().isEmpty());
        m_viewer.failOpen = false;
        pressSpace(m_view);
        QCOMPARE(nextCall().method, u"Open"_s);
        closeFromViewer();
    }

    // Separate runs (ctest: kwiklook-tst_dolphinhook-garbage / -crashed).
    void garbageTurnsOnTheFallback()
    {
        if (testCase() != u"garbage"_s) {
            QSKIP("runs as kwiklook-tst_dolphinhook-garbage");
        }
        m_view->selectOnly(1);
        m_view->container()->keysReceived.clear();
        const qsizetype before = m_viewer.calls.size();
        pressSpace(m_view); // the self-check fails: this Space stays Dolphin's
        QTest::qWait(200);
        QCOMPARE(m_viewer.calls.size(), before);
        QVERIFY(m_view->container()->keysReceived.contains(Qt::Key_Space));
        const QJsonObject s = bridgeState();
        QCOMPARE(s.value(u"check"_s).toString(), u"failed"_s);
        QVERIFY(s.value(u"problem"_s).toString().contains(u"unexpected answer"_s));
        QVERIFY(!s.value(u"modelAccess"_s).toBool());
        // From now on as without the internals: the selected file alone.
        m_view->selectOnly(3);
        pressSpace(m_view);
        const FakeViewer::Call call = nextCall();
        QCOMPARE(call.method, u"Open"_s);
        QCOMPARE(call.urls, QStringList{url(u"x3.png"_s).toString()});
        closeFromViewer();
        // Only this Dolphin process: nothing is remembered as broken.
        QVERIFY(guardEntry("Broken").isEmpty());
        QVERIFY(guardEntry("Checking").isEmpty());
    }

    void crashMarkerTurnsOnTheFallback()
    {
        if (testCase() != u"crashed"_s) {
            QSKIP("runs as kwiklook-tst_dolphinhook-crashed");
        }
        const QJsonObject s = bridgeState();
        QCOMPARE(s.value(u"check"_s).toString(), u"failed"_s);
        QVERIFY(s.value(u"problem"_s).toString().contains(u"“read”"_s));
        QVERIFY(!s.value(u"modelAccess"_s).toBool());
        QVERIFY(!guardEntry("Broken").isEmpty());
        QVERIFY(guardEntry("Checking").isEmpty());
        // Space previews the selected file alone, without touching the internals.
        focusView(m_view);
        m_view->selectOnly(3);
        pressSpace(m_view);
        const FakeViewer::Call call = nextCall();
        QCOMPARE(call.method, u"Open"_s);
        QCOMPARE(call.urls, QStringList{url(u"x3.png"_s).toString()});
        closeFromViewer();
    }
};

QTEST_MAIN(TstDolphinHook)
#include "tst_dolphinhook.moc"
