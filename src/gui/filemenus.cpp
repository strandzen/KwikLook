// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "filemenus.h"

#include <KIO/Global>
#include <KFileItemActions>
#include <KIO/ApplicationLauncherJob>
#include <KIO/CommandLauncherJob>
#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenFileManagerWindowJob>
#include <KIO/OpenUrlJob>
#include <KWaylandExtras>
#include <KWindowSystem>
#include <KLocalizedString>
#include <KService>
#include <Purpose/AlternativesModel>
#include <Purpose/Menu>

#include <QClipboard>

#include <algorithm>
#include <QCursor>
#include <QFuture>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QMenu>
#include <QMimeDatabase>
#include <QQuickItem>
#include <QQuickWindow>

using namespace Qt::StringLiterals;

FileMenus::FileMenus(QObject *parent)
    : QObject(parent)
{
}

FileMenus::~FileMenus() = default;

QString FileMenus::mimeType() const
{
    if (!m_mimeType.isEmpty()) {
        return m_mimeType;
    }
    // Not read yet (just switched files): by name only, without reading the file here.
    return QMimeDatabase().mimeTypeForFile(m_url.path(), QMimeDatabase::MatchExtension).name();
}

void FileMenus::showOpenWith(QQuickItem *button)
{
    if (QMenu *menu = openWithMenu()) {
        popup(menu, button);
    }
}

QMenu *FileMenus::openWithMenu()
{
    if (m_url.isEmpty()) {
        return nullptr;
    }
    auto *menu = new QMenu;
    menu->setAttribute(Qt::WA_DeleteOnClose);
    const QUrl url = m_url;
    const KService::List apps = KFileItemActions::associatedApplications({mimeType()});
    for (const KService::Ptr &app : apps) {
        QAction *action = menu->addAction(QIcon::fromTheme(app->icon()), app->name());
        const QString entry = app->desktopEntryName();
        connect(action, &QAction::triggered, this, [this, url, entry] {
            launch(url, entry);
        });
    }
    // Spectacle declares no file types, so it isn't among them: for pictures it opens
    // its annotation editor (crop, arrows, text, ...).
    const KService::Ptr spectacle = KService::serviceByDesktopName(u"org.kde.spectacle"_s);
    if (spectacle && url.isLocalFile() && mimeType().startsWith(u"image/"_s)
        && std::none_of(apps.cbegin(), apps.cend(), [](const KService::Ptr &app) {
               return app->desktopEntryName() == u"org.kde.spectacle"_s;
           })) {
        QAction *action = menu->addAction(QIcon::fromTheme(spectacle->icon()), spectacle->name());
        action->setObjectName(u"openWithSpectacle"_s);
        connect(action, &QAction::triggered, this, [this, url] {
            auto *job = new KIO::CommandLauncherJob(u"spectacle"_s, {u"--edit-existing"_s, url.toLocalFile()});
            job->setDesktopName(u"org.kde.spectacle"_s);
            job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, nullptr));
            connect(job, &KJob::result, this, [this](KJob *job) {
                if (!job->error()) {
                    Q_EMIT launched();
                }
            });
            job->start();
        });
    }
    if (!menu->actions().isEmpty()) {
        menu->addSeparator();
    }
    QAction *other = menu->addAction(QIcon::fromTheme(u"document-open"_s), i18nc("@action:inmenu", "Other Application…"));
    connect(other, &QAction::triggered, this, [this, url] {
        launch(url, QString());
    });
    return menu;
}

void FileMenus::openWithDefault()
{
    if (m_url.isEmpty()) {
        return;
    }
    auto *job = new KIO::OpenUrlJob(m_url, mimeType());
    job->setRunExecutables(false); // a script opens in its editor, it doesn't run
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, nullptr));
    connect(job, &KJob::result, this, [this](KJob *job) {
        if (!job->error()) {
            Q_EMIT launched();
        }
    });
    job->start();
}

void FileMenus::launch(const QUrl &url, const QString &desktopEntry)
{
    // No application: KIO asks which one (the standard “Open With” dialog).
    auto *job = desktopEntry.isEmpty() ? new KIO::ApplicationLauncherJob : new KIO::ApplicationLauncherJob(KService::serviceByDesktopName(desktopEntry));
    job->setUrls({url});
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, nullptr));
    connect(job, &KJob::result, this, [this](KJob *job) {
        if (!job->error()) {
            Q_EMIT launched();
        }
    });
    job->start();
}

void FileMenus::showShare(QQuickItem *button)
{
    if (m_url.isEmpty()) {
        return;
    }
    if (!m_shareMenu) {
        m_shareMenu = std::make_unique<Purpose::Menu>();
        m_shareMenu->model()->setPluginType(u"Export"_s);
        connect(m_shareMenu.get(), &Purpose::Menu::finished, this, [this](const QJsonObject &output, int error, const QString &errorMessage) {
            if (error == KIO::ERR_USER_CANCELED) {
                return;
            }
            if (error) {
                Q_EMIT message(i18n("Could not share the file: %1", errorMessage));
            } else if (const QString link = output.value(u"url"_s).toString(); !link.isEmpty()) {
                Q_EMIT message(i18n("Shared at %1", link));
            }
        });
    }
    m_shareMenu->model()->setInputData(QJsonObject{
        {u"urls"_s, QJsonArray{m_url.toString()}},
        {u"mimeType"_s, mimeType()},
    });
    m_shareMenu->reload();
    popup(m_shareMenu.get(), button);
}

void FileMenus::popup(QMenu *menu, QQuickItem *button)
{
    if (!button || !button->window()) {
        menu->popup(QCursor::pos());
        return;
    }
    // A Wayland popup needs its parent surface: the preview window. Style the menu
    // before creating its native window: Breeze makes menus translucent while
    // styling, and a window created earlier has no alpha (black corners).
    menu->ensurePolished();
    menu->winId();
    menu->windowHandle()->setTransientParent(button->window());
    menu->popup(button->mapToGlobal(QPointF(0, button->height())).toPoint());
}

void FileMenus::showInFileManager(QQuickItem *from)
{
    if (m_url.isEmpty()) {
        return;
    }
    const QUrl url = m_url;
    const auto show = [this, url](const QString &token) {
        auto *job = KIO::highlightInFileManager({url}, token.toUtf8());
        connect(job, &KJob::result, this, [this](KJob *job) {
            if (!job->error()) {
                Q_EMIT launched();
            }
        });
    };
    // On Wayland the file manager may take the focus only with a token from us.
    QWindow *window = from ? from->window() : nullptr;
    if (KWindowSystem::isPlatformWayland() && window) {
        KWaylandExtras::xdgActivationToken(window, u"org.kde.dolphin"_s).then(this, show);
    } else {
        show(QString());
    }
}

void FileMenus::copyPath()
{
    if (!m_url.isEmpty()) {
        QGuiApplication::clipboard()->setText(m_url.isLocalFile() ? m_url.toLocalFile() : m_url.toDisplayString());
    }
}
