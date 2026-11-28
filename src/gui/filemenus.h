// SPDX-License-Identifier: GPL-2.0-or-later
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
    // The “Open With” menu (deletes itself when closed): the type's applications,
    // Spectacle for pictures, “Other Application…”. Null without a file.
    QMenu *openWithMenu();
    // Opens the file in the application it opens with by default (Enter).
    Q_INVOKABLE void openWithDefault();
    // Opens the file in the application with this desktop file name, if it's installed.
    Q_INVOKABLE void openWith(const QString &desktopEntry);
    // That application's icon name; empty when it isn't installed.
    Q_INVOKABLE static QString applicationIcon(const QString &desktopEntry);
    Q_INVOKABLE void showShare(QQuickItem *button);
    // The share menu, filled for the current file: KDE's share targets, then LocalSend
    // when it's installed. Kept for reuse. Null without a file.
    QMenu *shareMenu();
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
    void addLocalSend(QMenu *menu);
    QString mimeType() const;

    QUrl m_url;
    QString m_mimeType;
    // Kept after the menu closes: the share job (and any dialog it shows) runs on.
    std::unique_ptr<Purpose::Menu> m_shareMenu;
};
