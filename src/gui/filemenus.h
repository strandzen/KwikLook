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
 * The title bar's “Open with…” and Share menus for a file, as KDE's own menus
 * (the applications for its type, KDE's share plugins), shown below a button.
 *
 *   FileMenus { id: menus; url: Viewer.currentUrl }
 *   ToolButton { onClicked: menus.showOpenWith(this) }
 */
class FileMenus : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QUrl url MEMBER m_url NOTIFY urlChanged)

public:
    explicit FileMenus(QObject *parent = nullptr);
    ~FileMenus() override;

    Q_INVOKABLE void showOpenWith(QQuickItem *button);
    Q_INVOKABLE void showShare(QQuickItem *button);

Q_SIGNALS:
    void urlChanged();
    // An application was started for the file.
    void launched();
    // Something to tell the user (a share failed, or gave a link).
    void message(const QString &text);

private:
    void popup(QMenu *menu, QQuickItem *button);
    void launch(const QUrl &url, const QString &desktopEntry);

    QUrl m_url;
    // Kept after the menu closes: the share job (and any dialog it shows) runs on.
    std::unique_ptr<Purpose::Menu> m_shareMenu;
};
