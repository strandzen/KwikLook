// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "kwinscript.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QFileInfo>
#include <QLoggingCategory>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcKWinScript, "kwiklook.kwinscript", QtInfoMsg)

namespace
{
constexpr int TimeoutMs = 2000;

QDBusMessage call(const QString &path, const QString &interface, const QString &method, const QVariantList &args = {})
{
    QDBusMessage message = QDBusMessage::createMethodCall(u"org.kde.KWin"_s, path, interface, method);
    message.setArguments(args);
    return QDBusConnection::sessionBus().call(message, QDBus::Block, TimeoutMs);
}

QDBusMessage scripting(const QString &method, const QVariantList &args = {})
{
    return call(u"/Scripting"_s, u"org.kde.kwin.Scripting"_s, method, args);
}

bool loadedInKWin()
{
    const QDBusMessage reply = scripting(u"isScriptLoaded"_s, {QString::fromLatin1(KWinScript::PluginName)});
    return reply.type() == QDBusMessage::ReplyMessage && reply.arguments().value(0).toBool();
}
} // namespace

KWinScript::KWinScript(const QString &path, QObject *parent)
    : QObject(parent)
    , m_path(path)
{
}

QString KWinScript::defaultPath()
{
    const QString local = QCoreApplication::applicationDirPath() + u"/kwiklook-placement.js"_s;
    if (QFileInfo::exists(local)) {
        return local;
    }
    return QStringLiteral(KWIKLOOK_DATADIR "/kwiklook/kwiklook-placement.js");
}

bool KWinScript::setLoaded(bool loaded)
{
    const QString name = QString::fromLatin1(PluginName);
    if (loaded == m_loaded) {
        return m_loaded;
    }
    if (!loaded || loadedInKWin()) {
        // Ours, or left by a viewer that crashed: its callbacks would reach this one.
        scripting(u"unloadScript"_s, {name});
        m_loaded = false;
    }
    if (!loaded) {
        return false;
    }
    if (!QFileInfo::exists(m_path)) {
        qCWarning(lcKWinScript) << "script not found:" << m_path;
        return false;
    }
    const QDBusMessage reply = scripting(u"loadScript"_s, {m_path, name});
    const int id = reply.arguments().value(0, -1).toInt();
    if (reply.type() != QDBusMessage::ReplyMessage || id < 0) {
        qCInfo(lcKWinScript) << "KWin did not load the window script; the preview is placed by the window manager alone" << reply.errorMessage();
        return false;
    }
    // Not run() on /Scripting/Script<id>: KWin numbers scripts by count, and a script it
    // unloaded can keep its path (seen on a live session), so that call can reach the old
    // one while ours never starts. start() runs every loaded script not yet running.
    const QDBusMessage run = scripting(u"start"_s);
    if (run.type() != QDBusMessage::ReplyMessage) {
        qCInfo(lcKWinScript) << "KWin did not run the window script:" << run.errorMessage();
        scripting(u"unloadScript"_s, {name});
        return false;
    }
    m_loaded = true;
    return true;
}

bool KWinScript::isLoaded() const
{
    return m_loaded;
}
