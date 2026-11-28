// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "sandbox.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QProcess>

#include <csignal>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcSandbox, "kwiklook.sandbox")

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

SandboxRun::SandboxRun(int timeoutMs, QObject *parent)
    : QObject(parent)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(timeoutMs);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        if (m_process) {
            qCWarning(lcSandbox) << "took too long, stopped:" << m_process->arguments().join(u' ');
        }
        end(End::TimedOut, -1);
    });
}

SandboxRun::~SandboxRun()
{
    cancel();
}

void SandboxRun::start(const QStringList &args, bool keepOutput)
{
    cancel();
    const QString sandbox = Sandbox::binary();
    auto *process = new QProcess(this);
    m_process = process;
    process->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    if (!keepOutput) {
        process->setStandardOutputFile(QProcess::nullDevice());
    }
    connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus status) {
        if (process != m_process) {
            return;
        }
        if (status != QProcess::NormalExit) {
            end(End::Failed, -1);
        } else {
            end(exitCode == Sandbox::ExitNoSandbox ? End::NoSandbox : End::Exited, exitCode);
        }
    });
    // Couldn't be started at all: no finished() follows, so end now, not at the time limit.
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (process == m_process && error == QProcess::FailedToStart) {
            qCWarning(lcSandbox) << "could not start" << process->program() << process->errorString();
            end(End::Failed, -1);
        }
    });
    m_timeout.start();
    process->start(sandbox.isEmpty() ? u"kwiklook-sandbox"_s : sandbox, args); // missing: FailedToStart
}

void SandboxRun::cancel()
{
    m_timeout.stop();
    QProcess *process = m_process;
    if (!process) {
        return;
    }
    m_process = nullptr;
    process->disconnect(this);
    if (process->state() == QProcess::NotRunning) {
        process->deleteLater();
        return;
    }
    if (process->processId() > 0) {
        ::kill(-pid_t(process->processId()), SIGKILL); // the sandbox's own process group
    }
    // Gone in a moment; deleted then, without waiting for it here (or when a start
    // that was still going fails: no finished() then).
    connect(process, &QProcess::finished, process, &QObject::deleteLater);
    connect(process, &QProcess::errorOccurred, process, [process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            process->deleteLater();
        }
    });
}

bool SandboxRun::isRunning() const
{
    return m_process;
}

void SandboxRun::end(End end, int exitCode)
{
    QProcess *process = m_process;
    if (!process) {
        return; // already ended (a start error, then the process's end)
    }
    // A crashed program's output so far is still worth having (what could be read).
    const QByteArray output = end != End::TimedOut ? process->readAllStandardOutput() : QByteArray();
    cancel(); // stops what's left (a timed-out run), lets the process go
    Q_EMIT finished(end, end == End::Exited ? exitCode : -1, output);
}
