// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include "dbusnames.h"
#include "fileinfo.h"

#include <QFutureWatcher>
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
    // About the current file, filled in shortly after it becomes current (read off the
    // main thread); empty until then.
    Q_PROPERTY(QString view READ view NOTIFY fileInfoChanged) // QML file that shows it
    Q_PROPERTY(QString mimeType READ mimeType NOTIFY fileInfoChanged)
    Q_PROPERTY(QString iconName READ iconName NOTIFY fileInfoChanged)
    Q_PROPERTY(bool fileExists READ fileExists NOTIFY fileInfoChanged)
    Q_PROPERTY(QString details READ details NOTIFY fileInfoChanged) // type · size · date
    Q_PROPERTY(QSize imageSize READ imageSize NOTIFY fileInfoChanged) // pictures; invalid otherwise
    Q_PROPERTY(QString text READ text NOTIFY fileInfoChanged) // text files: the start of the file
    // “This file is long: showing only its beginning.”, or empty.
    Q_PROPERTY(QString textNote READ textNote NOTIFY fileInfoChanged)
    // For the information panel: [{title, rows: [{label, value}]}], empty sections left out.
    Q_PROPERTY(QVariantList infoSections READ infoSections NOTIFY fileInfoChanged)
    // The information panel is open; remembered across previews and restarts.
    Q_PROPERTY(bool infoOpen READ infoOpen WRITE setInfoOpen NOTIFY infoOpenChanged)

public:
    static constexpr auto ServiceName = KwikLookDBus::Service;
    static constexpr auto ObjectPath = KwikLookDBus::ViewerPath;
    static constexpr auto InterfaceName = KwikLookDBus::ViewerInterface;

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
    QString view() const;
    QString mimeType() const;
    QString iconName() const;
    bool fileExists() const;
    QString details() const;
    QSize imageSize() const;
    QString text() const;
    QString textNote() const;
    QVariantList infoSections() const;
    bool infoOpen() const;
    void setInfoOpen(bool open);

public Q_SLOTS:
    // Shows `urls[index]` (local paths or URLs), with the others reachable by Next/Previous.
    // `activationToken` (Wayland) lets the preview take keyboard focus; may be empty.
    Q_SCRIPTABLE void Open(const QStringList &urls, int index, const QString &sessionId, const QString &activationToken);
    Q_SCRIPTABLE void Close();
    Q_SCRIPTABLE void Next();
    Q_SCRIPTABLE void Previous();
    // Ends the background viewer.
    Q_SCRIPTABLE void Quit();
    // Shows the settings window (`kwiklook --settings`, the “KwikLook Settings” launcher entry).
    Q_SCRIPTABLE void ShowSettings(const QString &activationToken);

Q_SIGNALS:
    Q_SCRIPTABLE void CurrentChanged(const QString &sessionId, const QString &url, int index);
    Q_SCRIPTABLE void Closed(const QString &sessionId);
    void currentChanged();
    void fileInfoChanged();
    void infoOpenChanged();
    // main.cpp creates the settings window on the first request and shows it.
    void settingsRequested(const QString &activationToken);

private:
    void setCurrent(int index);
    void showWindow(const QString &activationToken);
    void markClosed();
    void readFileInfo(int generation);
    void closeIfFocusLeft();

    QList<QUrl> m_urls;
    int m_index = -1;
    QString m_sessionId;
    bool m_open = false;
    QPointer<QWindow> m_window;
    FileInfo m_info;
    QString m_view; // kept while the next file's details load, so the view doesn't flicker
    bool m_infoOpen = false;
    int m_generation = 0; // drops details read for a file that's no longer current
};
