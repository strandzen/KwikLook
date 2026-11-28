// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "officeconverter.h"
#include "sandbox.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QStandardPaths>

#include <algorithm>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcOffice, "kwiklook.office")

OfficeConverter::OfficeConverter(QObject *parent)
    : QObject(parent)
    , m_cacheDir(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + u"/kwiklook/office"_s)
    , m_run(new SandboxRun(TimeoutMs, this))
{
    connect(m_run, &SandboxRun::finished, this, [this](SandboxRun::End end, int exitCode) {
        switch (end) {
        case SandboxRun::End::Exited:
            finish(exitCode, false);
            break;
        case SandboxRun::End::NoSandbox:
            finish(Sandbox::ExitNoSandbox, false);
            break;
        case SandboxRun::End::TimedOut:
            finish(-1, true);
            break;
        case SandboxRun::End::Failed:
            finish(-1, false);
            break;
        }
    });
}

OfficeConverter::~OfficeConverter()
{
    cancel();
}

bool OfficeConverter::available()
{
    return !QStandardPaths::findExecutable(u"soffice"_s).isEmpty() && !Sandbox::binary().isEmpty();
}

void OfficeConverter::setCacheDir(const QString &dir)
{
    m_cacheDir = dir;
}

void OfficeConverter::setCacheLimit(qint64 bytes)
{
    m_cacheLimit = bytes;
}

QString OfficeConverter::workDir() const
{
    return m_workDir;
}

QString OfficeConverter::cachedPdf(const QString &path) const
{
    // Same file, same size and date: same PDF. A changed file gets a new entry.
    const QFileInfo info(path);
    const QByteArray key = info.canonicalFilePath().toUtf8() + '\n' + QByteArray::number(info.size()) + '\n'
        + QByteArray::number(info.lastModified().toMSecsSinceEpoch());
    return m_cacheDir + u'/' + QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha1).toHex()) + u".pdf"_s;
}

void OfficeConverter::convert(const QString &path)
{
    cancel();
    const QString pdf = cachedPdf(path);
    if (QFileInfo::exists(pdf)) {
        // Used again: the newest in the cache, the last to be pruned.
        QFile file(pdf);
        if (file.open(QIODevice::ReadWrite)) {
            file.setFileTime(QDateTime::currentDateTime(), QFileDevice::FileModificationTime);
        }
        Q_EMIT converted(path, QUrl::fromLocalFile(pdf));
        return;
    }
    const QString sandbox = Sandbox::binary();
    const QString soffice = QStandardPaths::findExecutable(u"soffice"_s);
    if (sandbox.isEmpty() || soffice.isEmpty()) {
        Q_EMIT failed(path, Failure::Failed);
        return;
    }

    removeStaleWork();
    // A fresh scratch folder each time (profile included: nothing a document does
    // to LibreOffice's settings carries over to the next one).
    m_workDir = m_cacheDir + u"/work-"_s + QString::number(QCoreApplication::applicationPid());
    QDir(m_workDir).removeRecursively();
    for (const char *sub : {"home", "tmp", "out", "profile/user"}) {
        QDir().mkpath(m_workDir + u'/' + QLatin1String(sub));
    }
    writeProfileSettings(m_workDir + u"/profile/user/registrymodifications.xcu"_s);
    m_path = path;

    QStringList args{u"--read"_s, path, u"--write"_s, m_workDir,
                     // LibreOffice puts its own pipe in /tmp (hard-coded): sockets only.
                     u"--socket-dir"_s, u"/tmp"_s};
    // The user's own fonts, so documents look as they do in LibreOffice.
    const QString fonts = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + u"/fonts"_s;
    if (QFileInfo(fonts).isDir()) {
        args << u"--read"_s << fonts;
    }
    args << u"--env"_s << u"HOME=%1/home"_s.arg(m_workDir) << u"--env"_s << u"TMPDIR=%1/tmp"_s.arg(m_workDir);
    const QByteArray lang = qgetenv("LANG");
    if (!lang.isEmpty()) {
        args << u"--env"_s << u"LANG="_s + QString::fromLocal8Bit(lang);
    }
    args << u"--"_s << soffice << u"--headless"_s << u"--norestore"_s << u"--nolockcheck"_s
         << u"-env:UserInstallation=%1"_s.arg(QUrl::fromLocalFile(m_workDir + u"/profile"_s).toString()) << u"--convert-to"_s << u"pdf"_s
         << u"--outdir"_s << m_workDir + u"/out"_s << path;

    m_run->start(args, false); // the PDF lands in the scratch folder: no output kept
}

void OfficeConverter::writeProfileSettings(const QString &file)
{
    // Belt and braces next to the sandbox: no macros, and no updating of linked
    // files or data. (Writer: 2 = never; Calc's value 2 may mean “on request”, which
    // a headless conversion can't ask, so never too.) Finalized: a document can't
    // turn them back on.
    QFile xcu(file);
    if (!xcu.open(QIODevice::WriteOnly)) {
        qCWarning(lcOffice) << "could not write" << file;
        return;
    }
    xcu.write(R"(<?xml version="1.0" encoding="UTF-8"?>
<oor:items xmlns:oor="http://openoffice.org/2001/registry" xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
<item oor:path="/org.openoffice.Office.Common/Security/Scripting"><prop oor:name="DisableMacrosExecution" oor:op="fuse" oor:finalized="true"><value>true</value></prop></item>
<item oor:path="/org.openoffice.Office.Common/Security/Scripting"><prop oor:name="MacroSecurityLevel" oor:op="fuse" oor:finalized="true"><value>3</value></prop></item>
<item oor:path="/org.openoffice.Office.Writer/Content/Update"><prop oor:name="Link" oor:op="fuse" oor:finalized="true"><value>2</value></prop></item>
<item oor:path="/org.openoffice.Office.Calc/Content/Update"><prop oor:name="Link" oor:op="fuse" oor:finalized="true"><value>2</value></prop></item>
</oor:items>
)");
}

void OfficeConverter::removeStaleWork()
{
    // Scratch folders of earlier viewers that crashed or were killed mid-conversion
    // (only one viewer runs at a time, so any other is stale).
    const QString own = u"work-"_s + QString::number(QCoreApplication::applicationPid());
    const QStringList dirs = QDir(m_cacheDir).entryList({u"work-*"_s}, QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &dir : dirs) {
        if (dir != own) {
            QDir(m_cacheDir + u'/' + dir).removeRecursively(); // our own cache, not user files
        }
    }
}

bool OfficeConverter::sandboxSupported()
{
    return Sandbox::supported();
}

void OfficeConverter::cancel()
{
    m_run->cancel(); // LibreOffice's processes with it
    if (!m_workDir.isEmpty()) {
        QDir(m_workDir).removeRecursively();
        m_workDir.clear();
    }
    m_path.clear();
}

void OfficeConverter::finish(int exitCode, bool timedOut)
{
    if (m_path.isEmpty()) {
        return; // already finished (a start error, then the process's end)
    }
    const QString path = m_path;
    const QString out = m_workDir + u"/out/"_s + QFileInfo(path).completeBaseName() + u".pdf"_s;
    const QString pdf = cachedPdf(path);
    const bool ok = !timedOut && exitCode == 0 && QFileInfo::exists(out) && QFile::rename(out, pdf);
    if (!ok && exitCode != 0) {
        qCInfo(lcOffice) << "conversion failed, exit code" << exitCode << path;
    }
    cancel(); // stops what's left, removes the scratch folder
    if (ok) {
        prune();
        Q_EMIT converted(path, QUrl::fromLocalFile(pdf));
    } else {
        Q_EMIT failed(path, exitCode == Sandbox::ExitNoSandbox ? Failure::NoSandbox : Failure::Failed);
    }
}

void OfficeConverter::prune()
{
    QFileInfoList pdfs = QDir(m_cacheDir).entryInfoList({u"*.pdf"_s}, QDir::Files, QDir::Time); // newest first
    qint64 total = 0;
    for (const QFileInfo &pdf : std::as_const(pdfs)) {
        total += pdf.size();
        if (total > m_cacheLimit) {
            QFile::remove(pdf.filePath()); // a cache file of ours, not a user file
        }
    }
}
