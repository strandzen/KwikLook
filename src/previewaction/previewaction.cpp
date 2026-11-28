// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>

// “Preview” in the context menu of files, wherever KDE's file menus load
// plugins (Dolphin, Plasma's desktop and Folder View). Inside Dolphin it goes
// through the Dolphin hook, which knows the folder's order and selection and
// moves the entry up next to “Open With”; elsewhere it opens the files
// directly. Kept small: it is loaded into other applications (plasmashell).

#include "dbusnames.h"

#include <KAbstractFileItemActionPlugin>
#include <KFileItem>
#include <KFileItemListProperties>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KWaylandExtras>
#include <KWindowSystem>

#include <QAction>
#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QFuture>
#include <QPointer>
#include <QWidget>
#include <QWindow>

using namespace Qt::StringLiterals;

class PreviewAction : public KAbstractFileItemActionPlugin
{
    Q_OBJECT
public:
    PreviewAction(QObject *parent, const QVariantList &)
        : KAbstractFileItemActionPlugin(parent)
    {
    }

    QList<QAction *> actions(const KFileItemListProperties &props, QWidget *parentWidget) override
    {
        KFileItemList files;
        const KFileItemList items = props.items();
        for (const KFileItem &item : items) {
            if (!item.isDir()) {
                files << item;
            }
        }
        if (files.isEmpty()) {
            return {};
        }
        // Without a widget (Plasma's desktop) the plugin owns the action: only the latest.
        delete m_unowned;
        QObject *owner = parentWidget ? static_cast<QObject *>(parentWidget) : this;
        auto *action = new QAction(QIcon::fromTheme(u"document-preview"_s), i18nc("@action:inmenu", "Preview"), owner);
        if (!parentWidget) {
            m_unowned = action;
        }
        action->setObjectName(u"kwiklook_preview"_s); // the Dolphin hook places it by this name
        connect(action, &QAction::triggered, this, [this, files, widget = QPointer<QWidget>(parentWidget)] {
            preview(files, widget);
        });
        return {action};
    }

private:
    void preview(const KFileItemList &files, QWidget *widget)
    {
        // Inside Dolphin: the hook opens it like Space does, from the clicked file.
        if (QObject *bridge = qApp->findChild<QObject *>(QLatin1String(KwikLookDBus::BridgeObjectName), Qt::FindDirectChildrenOnly)) {
            bool handled = false;
            QMetaObject::invokeMethod(bridge, "previewFromMenu", Qt::DirectConnection, qReturnArg(handled), files.urlList());
            if (handled) {
                return;
            }
        }
        // Elsewhere: just these files, the first one shown.
        QStringList urls;
        for (const KFileItem &item : files) {
            urls << item.mostLocalUrl().toString();
        }
        // On Wayland the preview may take the focus only with a token from the window the user is in.
        QWindow *window = widget ? widget->window()->windowHandle() : QGuiApplication::focusWindow();
        if (KWindowSystem::isPlatformWayland() && window) {
            KWaylandExtras::xdgActivationToken(window, QLatin1String(KwikLookDBus::Service)).then(this, [urls](const QString &token) {
                open(urls, token);
            });
        } else {
            open(urls, QString());
        }
    }

    static void open(const QStringList &urls, const QString &activationToken)
    {
        QDBusMessage msg = QDBusMessage::createMethodCall(QLatin1String(KwikLookDBus::Service),
                                                          QLatin1String(KwikLookDBus::ViewerPath),
                                                          QLatin1String(KwikLookDBus::ViewerInterface),
                                                          u"Open"_s);
        msg.setArguments({urls, 0, QString(), activationToken});
        // Never block the application; D-Bus activation starts the viewer if needed.
        QDBusConnection::sessionBus().asyncCall(msg);
    }

    QPointer<QAction> m_unowned;
};

K_PLUGIN_CLASS_WITH_JSON(PreviewAction, "kwiklook_previewaction.json")

#include "previewaction.moc"
