// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "gui/kwinscript.h"
#include "gui/offlinenetwork.h"
#include "gui/thumbnailprovider.h"
#include "kwiklooksettings.h"
#include "gui/viewer.h"
#include "version.h"

#include <KAboutData>
#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <KWindowSystem>

#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDir>
#include <QIcon>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickStyle>
#include <QUrl>
#include <QtQml/qqmlextensionplugin.h>

#include <cstdio>

Q_IMPORT_QML_PLUGIN(kwiklookPlugin)

using namespace Qt::StringLiterals;

namespace
{
// The token a launcher gave us (Wayland), passed on so the preview can take focus.
QString takeActivationToken()
{
    const QString token = qEnvironmentVariable("XDG_ACTIVATION_TOKEN");
    qunsetenv("XDG_ACTIVATION_TOKEN");
    return token;
}

QDBusMessage viewerCall(const QString &method)
{
    return QDBusMessage::createMethodCall(QLatin1String(Viewer::ServiceName),
                                          QLatin1String(Viewer::ObjectPath),
                                          QLatin1String(Viewer::InterfaceName),
                                          method);
}
} // namespace

int main(int argc, char *argv[])
{
    // QApplication (not QGuiApplication): the desktop QtQuick Controls style needs it.
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("kwiklook");
    // KDE's style for Qt Quick controls, as KDE's own Kirigami apps do; without it
    // Kirigami falls back to basic colours (no separate title bar colours, say).
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(u"org.kde.desktop"_s);
    }

    KAboutData about(u"kwiklook"_s,
                     i18n("KwikLook"),
                     QStringLiteral(KWIKLOOK_VERSION_STRING),
                     i18n("Quick previews of files from Dolphin"),
                     KAboutLicense::GPL_V3,
                     i18n("© 2026 strandzen"));
    about.setDesktopFileName(u"io.github.strandzen.KwikLook"_s);
    KAboutData::setApplicationData(about);
    QApplication::setWindowIcon(QIcon::fromTheme(u"document-preview"_s));

    QCommandLineParser parser;
    about.setupCommandLine(&parser);
    parser.addPositionalArgument(u"files"_s, i18n("Files to preview."), u"[files…]"_s);
    const QCommandLineOption serviceOption(u"service"_s, i18n("Run in the background and wait for files to preview."));
    const QCommandLineOption quitOption(u"quit"_s, i18n("Stop the background viewer."));
    const QCommandLineOption settingsOption(u"settings"_s, i18n("Show the settings window."));
    parser.addOption(serviceOption);
    parser.addOption(quitOption);
    parser.addOption(settingsOption);
    parser.process(app);
    about.processCommandLine(&parser);

    QStringList urls;
    const auto files = parser.positionalArguments();
    for (const QString &f : files) {
        urls << QUrl::fromUserInput(f, QDir::currentPath(), QUrl::AssumeLocalFile).toString();
    }
    const QString token = takeActivationToken();

    auto bus = QDBusConnection::sessionBus();
    const bool running = bus.interface()->isServiceRegistered(QLatin1String(Viewer::ServiceName));

    // Another viewer runs already: hand the request over and leave.
    if (running && !parser.isSet(serviceOption)) {
        if (parser.isSet(quitOption)) {
            bus.call(viewerCall(u"Quit"_s));
        } else if (parser.isSet(settingsOption)) {
            QDBusMessage msg = viewerCall(u"ShowSettings"_s);
            msg << token;
            bus.call(msg);
        } else if (!urls.isEmpty()) {
            QDBusMessage msg = viewerCall(u"Open"_s);
            msg << urls << 0 << QString() << token;
            const QDBusMessage reply = bus.call(msg);
            if (reply.type() == QDBusMessage::ErrorMessage) {
                std::fprintf(stderr, "%s\n", qPrintable(reply.errorMessage()));
                return 1;
            }
        }
        return 0;
    }
    if (parser.isSet(quitOption)) {
        return 0; // nothing to stop
    }

    // Become the background viewer; it stays when its window closes.
    Viewer *viewer = Viewer::instance();
    if (!viewer->registerOnBus()) {
        std::fprintf(stderr, "%s\n", qPrintable(i18n("Another KwikLook viewer is starting; try again.")));
        return 1;
    }
    app.setQuitOnLastWindowClosed(false);

    // Saved settings tell other processes (the Dolphin hook watches the open key).
    for (KConfigSkeletonItem *item : KwikLookSettings::self()->items()) {
        item->setWriteFlags(KConfigBase::Notify);
    }

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    engine.addImageProvider(QLatin1String(ThumbnailProvider::Name), new ThumbnailProvider);
    engine.setNetworkAccessManagerFactory(OfflineNetworkFactory::instance()); // nothing fetched from the internet
    engine.loadFromModule("kwiklook", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    viewer->setWindow(qobject_cast<QWindow *>(engine.rootObjects().constFirst()));

    // The settings window is made on first use and kept (hidden) after that.
    QPointer<QWindow> settingsWindow;
    QObject::connect(viewer, &Viewer::settingsRequested, &app, [&engine, &settingsWindow](const QString &activationToken) {
        if (!settingsWindow) {
            QQmlComponent component(&engine, "kwiklook", "SettingsWindow");
            settingsWindow = qobject_cast<QWindow *>(component.create());
            if (!settingsWindow) {
                qWarning().noquote() << component.errorString();
                return;
            }
        }
        settingsWindow->show();
        if (!activationToken.isEmpty()) {
            KWindowSystem::setCurrentXdgActivationToken(activationToken);
        }
        KWindowSystem::activateWindow(settingsWindow);
    });

    // KwikLook's KWin script places the window and gives it the pictures' shape, unless
    // the setting leaves that to KWin. Follows the setting when it's saved.
    KWinScript placement(KWinScript::defaultPath());
    const auto updatePlacement = [viewer, &placement] {
        viewer->setPlacementActive(placement.setLoaded(KwikLookSettings::self()->windowPosition() != u"KWin"_s));
    };
    updatePlacement();
    QObject::connect(KwikLookSettings::self(), &KwikLookSettings::configChanged, &placement, updatePlacement);

    if (parser.isSet(settingsOption)) {
        viewer->ShowSettings(token);
    } else if (!urls.isEmpty()) {
        viewer->Open(urls, 0, QString(), token);
    }
    const int result = app.exec();
    placement.setLoaded(false);
    viewer->finishRendering(); // before the engine goes: see Known Pitfalls, Qt PDF
    return result;
}
