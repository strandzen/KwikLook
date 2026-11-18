// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "viewer.h"

#include <KWindowSystem>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcViewer, "kwiklook.viewer", QtInfoMsg)

namespace
{
Viewer *s_instance = nullptr;
}

Viewer::Viewer(QObject *parent)
    : QObject(parent)
{
}

Viewer *Viewer::instance()
{
    if (!s_instance) {
        s_instance = new Viewer(QCoreApplication::instance());
    }
    return s_instance;
}

Viewer *Viewer::create(QQmlEngine *, QJSEngine *)
{
    Viewer *viewer = instance();
    // Owned by the application, not by the QML engine (see CLAUDE.md pitfalls).
    QJSEngine::setObjectOwnership(viewer, QJSEngine::CppOwnership);
    return viewer;
}

bool Viewer::registerOnBus()
{
    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerObject(QLatin1String(ObjectPath), this, QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals)) {
        qCWarning(lcViewer) << "could not export" << ObjectPath;
        return false;
    }
    if (!bus.registerService(QLatin1String(ServiceName))) {
        bus.unregisterObject(QLatin1String(ObjectPath));
        return false;
    }
    return true;
}

void Viewer::setWindow(QWindow *window)
{
    m_window = window;
    if (window) {
        // Closed by the window manager (Alt+F4) or hidden from QML: same as Close().
        connect(window, &QWindow::visibleChanged, this, [this](bool visible) {
            if (!visible) {
                markClosed();
            }
        });
    }
}

QUrl Viewer::currentUrl() const
{
    return m_urls.value(m_index);
}

QString Viewer::fileName() const
{
    return currentUrl().fileName();
}

int Viewer::currentIndex() const
{
    return m_index;
}

int Viewer::count() const
{
    return m_urls.size();
}

bool Viewer::isOpen() const
{
    return m_open;
}

void Viewer::Open(const QStringList &urls, int index, const QString &sessionId, const QString &activationToken)
{
    QList<QUrl> parsed;
    parsed.reserve(urls.size());
    for (const QString &u : urls) {
        const QUrl url = QUrl::fromUserInput(u, QString(), QUrl::AssumeLocalFile);
        if (url.isValid() && !url.isEmpty()) {
            parsed << url;
        }
    }
    if (parsed.isEmpty()) {
        qCInfo(lcViewer) << "Open() without usable URLs ignored";
        return;
    }
    // A different caller takes over: tell the previous one its preview is gone.
    if (m_open && sessionId != m_sessionId) {
        Q_EMIT Closed(m_sessionId);
    }
    m_urls = parsed;
    m_sessionId = sessionId;
    m_open = true;
    m_index = -1;
    setCurrent(qBound(0, index, int(m_urls.size()) - 1));
    showWindow(activationToken);
}

void Viewer::Close()
{
    if (m_window && m_window->isVisible()) {
        m_window->hide(); // visibleChanged → markClosed()
    } else {
        markClosed();
    }
}

void Viewer::Next()
{
    if (m_open && m_index + 1 < m_urls.size()) {
        setCurrent(m_index + 1);
    }
}

void Viewer::Previous()
{
    if (m_open && m_index > 0) {
        setCurrent(m_index - 1);
    }
}

void Viewer::Quit()
{
    Close();
    QCoreApplication::quit();
}

void Viewer::setCurrent(int index)
{
    if (index == m_index) {
        return;
    }
    m_index = index;
    Q_EMIT currentChanged();
    Q_EMIT CurrentChanged(m_sessionId, currentUrl().toString(), m_index);
}

void Viewer::showWindow(const QString &activationToken)
{
    if (!m_window) {
        return;
    }
    m_window->show();
    if (!activationToken.isEmpty()) {
        KWindowSystem::setCurrentXdgActivationToken(activationToken);
    }
    KWindowSystem::activateWindow(m_window);
}

void Viewer::markClosed()
{
    if (!m_open) {
        return;
    }
    m_open = false;
    Q_EMIT Closed(m_sessionId);
}
