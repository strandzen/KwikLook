// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "gui/viewer.h"
#include "version.h"

#include <KAboutData>
#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDir>
#include <QIcon>
#include <QQmlApplicationEngine>
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
    parser.addOption(serviceOption);
    parser.addOption(quitOption);
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

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    engine.loadFromModule("kwiklook", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    viewer->setWindow(qobject_cast<QWindow *>(engine.rootObjects().constFirst()));

    if (!urls.isEmpty()) {
        viewer->Open(urls, 0, QString(), token);
    }
    return app.exec();
}
