// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

class QProcess;

// kwiklook-sandbox (src/sandbox): runs a converter locked in. Used for LibreOffice
// (OfficeConverter), the archive lister (ArchiveModel) and books (BookReader).
namespace Sandbox
{
// Its exit code when the kernel's Landlock is too old to lock a program in.
constexpr int ExitNoSandbox = 77;

// Next to the binary in the build tree, else where it's installed; empty if missing.
QString binary();

// The helper is there and this kernel can lock a program in (asked once).
bool supported();
}

/*
 * One program run inside kwiklook-sandbox, as OfficeConverter, BookReader and
 * ArchiveModel need it: a time limit; cancel() kills the sandbox's whole process
 * group (it starts its own: LibreOffice runs as several processes) without waiting
 * for it; exactly one finished() per run that isn't cancelled. Error output goes to
 * the viewer's own (the journal).
 */
class SandboxRun : public QObject
{
    Q_OBJECT
public:
    enum class End {
        Exited, // ended by itself: see exitCode
        NoSandbox, // the kernel can't lock it in (Sandbox::ExitNoSandbox): it didn't run
        TimedOut, // stopped at the time limit
        Failed, // the sandbox couldn't be started, or crashed
    };
    Q_ENUM(End)

    SandboxRun(int timeoutMs, QObject *parent);
    ~SandboxRun() override;

    // Starts kwiklook-sandbox with `args` (its options, "--", the program and its
    // arguments); cancels a run still going. Standard output is kept for finished()
    // unless `keepOutput` is false.
    void start(const QStringList &args, bool keepOutput = true);
    // Stops the run, if any; no finished() for it.
    void cancel();
    bool isRunning() const;

Q_SIGNALS:
    // `exitCode` is the program's for End::Exited, else -1; `output` is its standard
    // output (also what a crashed one wrote; nothing after a time-out).
    void finished(SandboxRun::End end, int exitCode, const QByteArray &output);

private:
    void end(End end, int exitCode);

    QPointer<QProcess> m_process;
    QTimer m_timeout;
};
