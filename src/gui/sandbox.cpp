// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "sandbox.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>

using namespace Qt::StringLiterals;

QString Sandbox::binary()
{
    const QString local = QCoreApplication::applicationDirPath() + u"/kwiklook-sandbox"_s;
    if (QFileInfo(local).isExecutable()) {
        return local;
    }
    const QString installed = QStringLiteral(KWIKLOOK_LIBEXECDIR "/kwiklook-sandbox");
    return QFileInfo(installed).isExecutable() ? installed : QString();
}

bool Sandbox::supported()
{
    static const bool supported = [] {
        const QString sandbox = binary();
        if (sandbox.isEmpty()) {
            return false;
        }
        QProcess process;
        process.start(sandbox, {u"--"_s, u"true"_s});
        return process.waitForFinished(5000) && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    }();
    return supported;
}
