// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "officeconverter.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QProcess>
#include <QStandardPaths>

#include <algorithm>
#include <csignal>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcOffice, "kwiklook.office")

namespace
{
constexpr int ExitNoSandbox = 77; // kwiklook-sandbox: the kernel can't lock the program in

// Next to the binary in the build tree, else where it's installed.
QString sandboxBinary()
{
    const QString local = QCoreApplication::applicationDirPath() + u"/kwiklook-sandbox"_s;
    if (QFileInfo(local).isExecutable()) {
        return local;
    }
    const QString installed = QStringLiteral(KWIKLOOK_LIBEXECDIR "/kwiklook-sandbox");
    return QFileInfo(installed).isExecutable() ? installed : QString();
}
} // namespace

OfficeConverter::OfficeConverter(QObject *parent)
    : QObject(parent)
    , m_cacheDir(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + u"/kwiklook/office"_s)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(TimeoutMs);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        qCWarning(lcOffice) << "conversion took too long, stopped:" << m_path;
        finish(-1, true);
    });
}

OfficeConverter::~OfficeConverter()
{
    cancel();
}

bool OfficeConverter::available()
{
    return !QStandardPaths::findExecutable(u"soffice"_s).isEmpty() && !sandboxBinary().isEmpty();
}

void OfficeConverter::setCacheDir(const QString &dir)
{
    m_cacheDir = dir;
}

QString OfficeConverter::cacheDir() const
{
    return m_cacheDir;
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
    const QString sandbox = sandboxBinary();
    const QString soffice = QStandardPaths::findExecutable(u"soffice"_s);
    if (sandbox.isEmpty() || soffice.isEmpty()) {
        Q_EMIT failed(path, Failure::Failed);
        return;
    }

    // A fresh scratch folder each time (profile included: nothing a document does
    // to LibreOffice's settings carries over to the next one).
    m_workDir = m_cacheDir + u"/work-"_s + QString::number(QCoreApplication::applicationPid());
    QDir(m_workDir).removeRecursively();
    for (const char *sub : {"home", "tmp", "out"}) {
        QDir().mkpath(m_workDir + u'/' + QLatin1String(sub));
    }
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

    auto *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    process->setStandardOutputFile(QProcess::nullDevice());
    connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus status) {
        if (process != m_process) {
            return; // cancelled
        }
        finish(status == QProcess::NormalExit ? exitCode : -1, false);
    });
    m_process = process;
    process->start(sandbox, args);
    m_timeout.start();
}

void OfficeConverter::cancel()
{
    m_timeout.stop();
    if (QProcess *process = m_process) {
        m_process = nullptr;
        // The sandbox starts its own process group: LibreOffice runs as several processes.
        if (process->processId() > 0) {
            ::kill(-pid_t(process->processId()), SIGKILL);
        }
        process->waitForFinished(1000);
        process->deleteLater();
    }
    if (!m_workDir.isEmpty()) {
        QDir(m_workDir).removeRecursively();
        m_workDir.clear();
    }
    m_path.clear();
}

void OfficeConverter::finish(int exitCode, bool timedOut)
{
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
        Q_EMIT failed(path, exitCode == ExitNoSandbox ? Failure::NoSandbox : Failure::Failed);
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
