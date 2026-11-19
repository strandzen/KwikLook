// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QQuickItem>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include <memory>

class QMenu;

namespace Purpose
{
class Menu;
}

/*
 * What can be done with the previewed file: the title bar's “Open with…” and
 * Share menus, as KDE's own menus (the applications for its type, KDE's share
 * plugins) shown below a button, and the information panel's actions.
 *
 *   FileMenus { id: menus; url: Viewer.currentUrl; mimeType: Viewer.mimeType }
 *   ToolButton { onClicked: menus.showOpenWith(this) }
 */
class FileMenus : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QUrl url MEMBER m_url NOTIFY urlChanged)
    // The file's type, when already known (Viewer.mimeType); otherwise guessed from the name.
    Q_PROPERTY(QString mimeType MEMBER m_mimeType NOTIFY mimeTypeChanged)

public:
    explicit FileMenus(QObject *parent = nullptr);
    ~FileMenus() override;

    Q_INVOKABLE void showOpenWith(QQuickItem *button);
    Q_INVOKABLE void showShare(QQuickItem *button);
    // Opens the file manager at the file's folder with the file selected.
    Q_INVOKABLE void showInFileManager(QQuickItem *from);
    // The local path (or the URL of a remote file) to the clipboard.
    Q_INVOKABLE void copyPath();

Q_SIGNALS:
    void urlChanged();
    void mimeTypeChanged();
    // An application (or the file manager) was started for the file.
    void launched();
    // Something to tell the user (a share failed, or gave a link).
    void message(const QString &text);

private:
    void popup(QMenu *menu, QQuickItem *button);
    void launch(const QUrl &url, const QString &desktopEntry);
    QString mimeType() const;

    QUrl m_url;
    QString m_mimeType;
    // Kept after the menu closes: the share job (and any dialog it shows) runs on.
    std::unique_ptr<Purpose::Menu> m_shareMenu;
};
