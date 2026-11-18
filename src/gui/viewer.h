// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QtQml/qqmlregistration.h>
#include <QUrl>
#include <QWindow>

/*
 * The background viewer: the list of files being previewed, the current one,
 * and the preview window. Published on the session bus as
 * io.github.strandzen.KwikLook /Viewer; the Dolphin hook and `kwiklook <files>`
 * talk to it only through that interface.
 *
 * Session IDs are chosen by the caller (one per Dolphin window, say) and come
 * back in the signals, so each caller can ignore what another one started.
 */
class Viewer : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.strandzen.KwikLook.Viewer")
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QUrl currentUrl READ currentUrl NOTIFY currentChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY currentChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentChanged)
    Q_PROPERTY(int count READ count NOTIFY currentChanged)

public:
    static constexpr auto ServiceName = "io.github.strandzen.KwikLook";
    static constexpr auto ObjectPath = "/Viewer";
    static constexpr auto InterfaceName = "io.github.strandzen.KwikLook.Viewer";

    // No default argument on purpose: a default-constructible QML_SINGLETON is
    // built by the engine itself and create() is never called (see CLAUDE.md).
    explicit Viewer(QObject *parent);

    // The one viewer of the running app (also what QML gets).
    static Viewer *instance();
    static Viewer *create(QQmlEngine *, QJSEngine *);

    // Exports the object and takes the service name. False if another viewer has it.
    bool registerOnBus();
    // The preview window; shown on Open(), hidden on Close().
    void setWindow(QWindow *window);

    QUrl currentUrl() const;
    QString fileName() const;
    int currentIndex() const;
    int count() const;
    bool isOpen() const;

public Q_SLOTS:
    // Shows `urls[index]` (local paths or URLs), with the others reachable by Next/Previous.
    // `activationToken` (Wayland) lets the preview take keyboard focus; may be empty.
    Q_SCRIPTABLE void Open(const QStringList &urls, int index, const QString &sessionId, const QString &activationToken);
    Q_SCRIPTABLE void Close();
    Q_SCRIPTABLE void Next();
    Q_SCRIPTABLE void Previous();
    // Ends the background viewer.
    Q_SCRIPTABLE void Quit();

Q_SIGNALS:
    Q_SCRIPTABLE void CurrentChanged(const QString &sessionId, const QString &url, int index);
    Q_SCRIPTABLE void Closed(const QString &sessionId);
    void currentChanged();

private:
    void setCurrent(int index);
    void showWindow(const QString &activationToken);
    void markClosed();

    QList<QUrl> m_urls;
    int m_index = -1;
    QString m_sessionId;
    bool m_open = false;
    QPointer<QWindow> m_window;
};
