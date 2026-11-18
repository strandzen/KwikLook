// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "autostart.h"

#include <KLocalizedString>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcAutostart, "kwiklook.autostart", QtInfoMsg)

Autostart::Autostart(QObject *parent)
    : QObject(parent)
{
}

QString Autostart::filePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + u"/autostart/io.github.strandzen.KwikLook.desktop"_s;
}

bool Autostart::isEnabled() const
{
    return QFileInfo::exists(filePath());
}

bool Autostart::setEnabled(bool enabled)
{
    if (enabled == isEnabled()) {
        return true;
    }
    const QString path = filePath();
    if (!enabled) {
        // Our own generated file, not a user file: removed, not trashed.
        if (!QFile::remove(path)) {
            qCWarning(lcAutostart) << "could not remove" << path;
            return false;
        }
        Q_EMIT enabledChanged();
        return true;
    }
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        qCWarning(lcAutostart) << "could not create the folder for" << path;
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(lcAutostart) << "could not write" << path << file.errorString();
        return false;
    }
    // Found through PATH, so it's the installed program, not whatever build wrote it.
    const QString entry = u"[Desktop Entry]\n"
                          "Type=Application\n"
                          "Name=%1\n"
                          "Comment=%2\n"
                          "Exec=kwiklook --service\n"
                          "Icon=document-preview\n"
                          "Terminal=false\n"_s.arg(i18n("KwikLook"), i18n("Get file previews ready in the background"));
    file.write(entry.toUtf8());
    if (!file.commit()) {
        qCWarning(lcAutostart) << "could not write" << path << file.errorString();
        return false;
    }
    Q_EMIT enabledChanged();
    return true;
}
