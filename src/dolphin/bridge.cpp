// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 KDEGlance contributors
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
// Based on KDEGlance's Dolphin hook (https://github.com/AlexGraveler/KDEGlance).
#include "bridge.h"

#include "dbusnames.h"
#include "dolphinaccess.h"
#include "openkey.h"

#include <KWaylandExtras>
#include <KWindowSystem>

#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QFuture>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLoggingCategory>
#include <QScopeGuard>
#include <QWidget>
#include <QWindow>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcBridge, "kwiklook.dolphin", QtInfoMsg)

namespace
{
QPointer<Bridge> s_bridge;
constexpr int TypeAheadFallbackMs = 1000; // Dolphin's type-ahead timeout
constexpr auto AppId = "io.github.strandzen.KwikLook"; // the window to activate

} // namespace

Bridge *Bridge::ensure()
{
    if (s_bridge) {
        return s_bridge;
    }
    auto *app = qobject_cast<QApplication *>(QCoreApplication::instance());
    // Only Dolphin itself (not Konqueror's embedded view, not file dialogs).
    if (!app || QCoreApplication::applicationName() != u"dolphin"_s) {
        return nullptr;
    }
    // Emergency switch: KWIKLOOK_DISABLE_DOLPHIN_HOOK=1 dolphin
    if (qEnvironmentVariableIntValue("KWIKLOOK_DISABLE_DOLPHIN_HOOK") != 0) {
        qCInfo(lcBridge) << "KwikLook's Dolphin integration disabled by KWIKLOOK_DISABLE_DOLPHIN_HOOK";
        return nullptr;
    }
    s_bridge = new Bridge(app);
    return s_bridge;
}

Bridge::Bridge(QObject *parent)
    : QObject(parent)
{
    qApp->installEventFilter(this);
    connect(qApp, &QCoreApplication::aboutToQuit, this, &Bridge::shutdown);

    // The open key from KwikLook's settings; the settings window saves with Notify.
    m_config = KSharedConfig::openConfig(QLatin1String(OpenKey::ConfigFile), KConfig::SimpleConfig);
    readOpenKey();
    m_configWatcher = KConfigWatcher::create(m_config);
    connect(m_configWatcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group, const QByteArrayList &names) {
        if (group.name() == QLatin1String(OpenKey::ConfigGroup) && names.contains(OpenKey::ConfigKey)) {
            readOpenKey();
        }
    });

    auto bus = QDBusConnection::sessionBus();
    bus.registerObject(QLatin1String(KwikLookDBus::BridgePath), this, QDBusConnection::ExportScriptableSlots);
    const QString service = QLatin1String(KwikLookDBus::Service);
    const QString path = QLatin1String(KwikLookDBus::ViewerPath);
    const QString iface = QLatin1String(KwikLookDBus::ViewerInterface);
    bus.connect(service, path, iface, u"CurrentChanged"_s, this, SLOT(onViewerCurrentChanged(QString, QString, int)));
    bus.connect(service, path, iface, u"Closed"_s, this, SLOT(onViewerClosed(QString)));
    // A viewer that quits or crashes sends no Closed: end the session, or Space
    // would keep trying to close a preview that isn't there.
    auto *watcher = new QDBusServiceWatcher(service, bus, QDBusServiceWatcher::WatchForUnregistration, this);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, &Bridge::endSession);

    const DolphinAccess &access = DolphinAccess::instance();
    if (!access.hasModelAccess()) {
        startFallback();
    }

    // Start the viewer now (D-Bus activation), without waiting, so the first
    // preview opens at once.
    QDBusMessage start = QDBusMessage::createMethodCall(u"org.freedesktop.DBus"_s, u"/org/freedesktop/DBus"_s, u"org.freedesktop.DBus"_s, u"StartServiceByName"_s);
    start << service << 0u;
    bus.asyncCall(start);

    qCInfo(lcBridge) << "KwikLook's Dolphin integration active in Dolphin" << QCoreApplication::applicationVersion()
                     << "; model access:" << access.hasModelAccess() << "selection sync:" << access.hasSelectionAccess();
}

void Bridge::startFallback()
{
    if (m_fallbackStarted) {
        return;
    }
    m_fallbackStarted = true;
    // Without model access, remember each view's last selection through
    // DolphinView's public selectionChanged(KFileItemList) signal.
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        if (QWidget *view = DolphinAccess::viewFor(now)) {
            trackFallbackSelection(view);
        }
    });
}

Bridge::~Bridge()
{
    shutdown();
}

void Bridge::shutdown()
{
    if (m_down) {
        return;
    }
    m_down = true;
    // Also reached from ~QCoreApplication (if Dolphin exits without
    // aboutToQuit): touch the application and the bus only while they exist.
    if (QCoreApplication *app = QCoreApplication::instance()) {
        app->removeEventFilter(this);
        QDBusConnection::sessionBus().unregisterObject(QLatin1String(KwikLookDBus::BridgePath));
    }
}

bool Bridge::canHandleKeyIn(QWidget *view)
{
    const DolphinAccess &access = DolphinAccess::instance();
    const bool hadModelAccess = access.hasModelAccess();
    const auto useFallbackIfChanged = qScopeGuard([&] {
        if (hadModelAccess && !access.hasModelAccess()) {
            // The self-check (first use) just failed: from now on as without the internals.
            startFallback();
            trackFallbackSelection(view);
        }
    });
    bool known = false;
    bool typeAhead = access.isTypeAheadActive(view, &known);
    if (!known) {
        typeAhead = m_lastTypeAheadKey.isValid() && m_lastTypeAheadKey.elapsed() < TypeAheadFallbackMs;
    }
    if (typeAhead) {
        return false; // the user is typing a file name that contains a space
    }
    if (access.isSelectionModeActive(view)) {
        return false; // Space toggles items in Dolphin's selection mode
    }
    if (access.hasModelAccess()) {
        const bool target = access.hasFileTarget(view);
        if (access.hasModelAccess()) {
            return target;
        }
    }
    // Fallback: a remembered selection with a file in it.
    const KFileItemList selection = m_fallbackSelection.value(view);
    return std::any_of(selection.cbegin(), selection.cend(), [](const KFileItem &i) {
        return !i.isDir();
    });
}

void Bridge::readOpenKey()
{
    m_config->reparseConfiguration();
    const QString text = m_config->group(QLatin1String(OpenKey::ConfigGroup)).readEntry(OpenKey::ConfigKey, QString());
    m_openKey = OpenKey::combination(text);
    qCDebug(lcBridge) << "open key" << OpenKey::normalized(text);
}

bool Bridge::eventFilter(QObject *watched, QEvent *event)
{
    const QEvent::Type type = event->type();
    if (m_down || (type != QEvent::KeyPress && type != QEvent::ShortcutOverride)) {
        return false;
    }
    QWidget *focus = QApplication::focusWidget();
    if (watched != focus) {
        return false; // only the first delivery, to the focus widget
    }
    auto *ke = static_cast<QKeyEvent *>(event);
    if (OpenKey::eventCombination(Qt::Key(ke->key()), ke->modifiers()) != m_openKey) {
        // Remember typing, for Dolphins whose type-ahead state can't be read.
        if (type == QEvent::KeyPress && !(ke->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
            const QString text = ke->text();
            if (!text.isEmpty() && text.at(0).isPrint() && !text.at(0).isSpace() && DolphinAccess::isFileViewFocus(focus)) {
                m_lastTypeAheadKey.start();
            }
        }
        return false;
    }
    QWidget *view = nullptr;
    if (!DolphinAccess::isFileViewFocus(focus, &view) || !canHandleKeyIn(view)) {
        return false;
    }
    if (type == QEvent::ShortcutOverride) {
        // Claim the key so no Dolphin shortcut fires.
        event->accept();
        return true;
    }
    if (!ke->isAutoRepeat()) {
        trigger(view);
    }
    return true;
}

bool Bridge::trigger(QWidget *view)
{
    // Space again in the view that has a preview open closes it (when Dolphin
    // kept the focus; normally the preview has it and closes itself).
    if (m_sessionView == view && !m_sessionId.isEmpty()) {
        if (m_tokenSession == m_sessionId) {
            // The preview hasn't been asked to open yet (waiting for the
            // activation token): drop the open instead of closing.
            endSession();
        } else {
            callViewer(u"Close"_s, {});
        }
        return true;
    }

    const DolphinAccess &access = DolphinAccess::instance();
    QStringList urls;
    QHash<QString, QUrl> localToModel;
    int index = 0;
    bool selectionOnly = false;
    if (auto snap = access.snapshot(view)) {
        urls.reserve(snap->urls.size());
        for (int i = 0; i < snap->urls.size(); ++i) {
            const QString local = snap->urls.at(i).toString();
            urls << local;
            localToModel.insert(local, snap->modelUrls.at(i));
        }
        index = snap->current;
        selectionOnly = snap->selectionOnly;
    } else if (!access.hasModelAccess()) {
        const KFileItemList selection = m_fallbackSelection.value(view);
        for (const KFileItem &item : selection) {
            if (!item.isDir()) {
                urls << item.mostLocalUrl().toString();
                break;
            }
        }
    }
    if (urls.isEmpty()) {
        return false;
    }

    if (m_sessionView && m_sessionView != view) {
        disconnect(m_sessionView, SIGNAL(selectionChanged(KFileItemList)), this, SLOT(onSessionSelectionChanged(KFileItemList)));
    }
    m_sessionId = u"%1/%2"_s.arg(QDBusConnection::sessionBus().baseService()).arg(++m_sessionCounter);
    m_sessionView = view;
    m_sessionUrls = urls;
    m_localToModelUrl = localToModel;
    m_viewerCurrent = urls.value(index);
    m_selectionOnly = selectionOnly;
    // If Dolphin keeps the keyboard focus (e.g. the compositor refused to
    // activate the preview), the preview follows Dolphin's selection.
    connect(view, SIGNAL(selectionChanged(KFileItemList)), this, SLOT(onSessionSelectionChanged(KFileItemList)), Qt::UniqueConnection);

    // On Wayland the preview may take the focus only with a token from the
    // window the user is in.
    QWindow *window = view->window()->windowHandle();
    if (KWindowSystem::isPlatformWayland() && window) {
        const QString session = m_sessionId;
        m_tokenSession = session;
        KWaylandExtras::xdgActivationToken(window, QLatin1String(AppId)).then(this, [this, session, index](const QString &token) {
            if (session == m_tokenSession) {
                m_tokenSession.clear();
            }
            if (session == m_sessionId) {
                open(index, token);
            }
        });
    } else {
        open(index, QString());
    }
    return true;
}

void Bridge::open(int index, const QString &activationToken)
{
    callViewer(u"Open"_s, {m_sessionUrls, index, m_sessionId, activationToken});
}

void Bridge::callViewer(const QString &method, const QVariantList &args)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(QLatin1String(KwikLookDBus::Service),
                                                      QLatin1String(KwikLookDBus::ViewerPath),
                                                      QLatin1String(KwikLookDBus::ViewerInterface),
                                                      method);
    msg.setArguments(args);
    // Never block Dolphin: activation starts the viewer if needed, and the reply
    // is only looked at for errors.
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg, 10000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, method, session = m_sessionId](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        // The viewer never got it (not installed, didn't start, timed out): no
        // preview is open, so the next Space must open one, not send Close.
        if (w->isError() && !session.isEmpty() && session == m_sessionId) {
            qCWarning(lcBridge) << method << "failed, preview ended:" << w->error().name() << w->error().message();
            endSession();
        }
    });
}

void Bridge::syncSelection(const QString &url)
{
    // Going through a selection of files keeps that selection in Dolphin.
    if (!m_sessionView || m_selectionOnly) {
        return;
    }
    const QUrl modelUrl = m_localToModelUrl.value(url, QUrl(url));
    m_applyingSync = true;
    DolphinAccess::instance().selectUrl(m_sessionView, modelUrl);
    m_applyingSync = false;
}

void Bridge::onViewerCurrentChanged(const QString &session, const QString &url, int index)
{
    Q_UNUSED(index)
    if (session != m_sessionId || m_sessionId.isEmpty()) {
        return; // another Dolphin window/process opened that preview
    }
    m_viewerCurrent = url;
    syncSelection(url);
}

void Bridge::onViewerClosed(const QString &session)
{
    if (session != m_sessionId || m_sessionId.isEmpty()) {
        return;
    }
    if (m_sessionView) {
        // Give the keyboard focus back to the file view inside Dolphin; the
        // compositor reactivates Dolphin's window.
        if (QWidget *container = DolphinAccess::containerOf(m_sessionView)) {
            container->setFocus(Qt::OtherFocusReason);
        }
    }
    endSession();
}

void Bridge::endSession()
{
    m_sessionId.clear();
    m_viewerCurrent.clear();
    if (m_sessionView) {
        disconnect(m_sessionView, SIGNAL(selectionChanged(KFileItemList)), this, SLOT(onSessionSelectionChanged(KFileItemList)));
    }
}

void Bridge::onSessionSelectionChanged(const KFileItemList &selection)
{
    if (m_applyingSync || m_sessionId.isEmpty() || sender() != m_sessionView || selection.size() != 1 || selection.first().isDir()) {
        return;
    }
    const QString url = selection.first().mostLocalUrl().toString();
    const qsizetype index = m_sessionUrls.indexOf(url);
    if (url == m_viewerCurrent || index < 0) {
        return;
    }
    m_viewerCurrent = url;
    open(int(index), QString());
}

void Bridge::trackFallbackSelection(QWidget *view)
{
    if (m_trackedViews.contains(view)) {
        return;
    }
    if (connect(view, SIGNAL(selectionChanged(KFileItemList)), this, SLOT(onFallbackSelectionChanged(KFileItemList)))) {
        m_trackedViews.insert(view);
        connect(view, &QObject::destroyed, this, [this, view] {
            m_trackedViews.remove(view);
            m_fallbackSelection.remove(view);
        });
    }
}

void Bridge::onFallbackSelectionChanged(const KFileItemList &selection)
{
    if (auto *view = qobject_cast<QWidget *>(sender())) {
        m_fallbackSelection.insert(view, selection);
    }
}

QWidget *Bridge::activeFileView() const
{
    QWidget *view = nullptr;
    if (DolphinAccess::isFileViewFocus(QApplication::focusWidget(), &view)) {
        return view;
    }
    // Dolphin is not the active window (e.g. a test runs in a terminal): use
    // the file view a Dolphin window would give the focus back to.
    const auto windows = QApplication::topLevelWidgets();
    for (QWidget *w : windows) {
        if (w->isVisible() && w->inherits("DolphinMainWindow") && DolphinAccess::isFileViewFocus(w->focusWidget(), &view)) {
            return view;
        }
    }
    return nullptr;
}

QString Bridge::State()
{
    const DolphinAccess &access = DolphinAccess::instance();
    QJsonObject o{
        {u"modelAccess"_s, access.hasModelAccess()},
        {u"selectionAccess"_s, access.hasSelectionAccess()},
        {u"missingSymbols"_s, QJsonArray::fromStringList(access.missingSymbols())},
        {u"check"_s, access.checkState()},
        {u"problem"_s, access.problem()},
        {u"session"_s, m_sessionId},
        {u"sessionViewAlive"_s, !m_sessionView.isNull()},
        {u"dolphinVersion"_s, QCoreApplication::applicationVersion()},
        {u"openKey"_s, QKeySequence(m_openKey).toString(QKeySequence::PortableText)},
    };
    QWidget *focusView = nullptr;
    o.insert(u"focusInFileView"_s, DolphinAccess::isFileViewFocus(QApplication::focusWidget(), &focusView));
    if (QWidget *view = m_sessionView ? m_sessionView.data() : activeFileView()) {
        if (auto snap = access.snapshot(view)) {
            o.insert(u"viewFileCount"_s, int(snap->urls.size()));
            o.insert(u"target"_s, snap->urls.value(snap->current).toString());
        }
    }
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

bool Bridge::TriggerFocusedView()
{
    QWidget *view = activeFileView();
    return view && trigger(view);
}
