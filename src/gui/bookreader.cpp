// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "bookreader.h"

#include "archivemodel.h"
#include "sandbox.h"

#include <QFutureWatcher>
#include <QLoggingCategory>
#include <QProcess>
#include <QtConcurrentRun>

#include <csignal>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcBook, "kwiklook.book", QtInfoMsg)

BookReader::BookReader(QObject *parent)
    : QObject(parent)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(TimeoutMs);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        const QString path = m_path;
        stop();
        Q_EMIT failed(path, Failure::Unreadable);
    });
}

BookReader::~BookReader()
{
    stop();
}

void BookReader::read(const QString &path)
{
    stop();
    m_path = path;
    const int generation = ++m_generation;
    const QString sandbox = Sandbox::binary();
    const QString helper = ArchiveModel::helper();
    if (sandbox.isEmpty() || helper.isEmpty() || !Sandbox::supported()) {
        QMetaObject::invokeMethod(this, [this, path] { Q_EMIT failed(path, Failure::NoSandbox); }, Qt::QueuedConnection);
        return;
    }
    auto *process = new QProcess(this);
    m_process = process;
    connect(process, &QProcess::finished, this, [this, process, path, generation](int exitCode) {
        if (process != m_process) {
            return;
        }
        m_timeout.stop();
        m_process = nullptr;
        const QByteArray output = process->readAllStandardOutput();
        process->deleteLater();
        if (exitCode == Sandbox::ExitNoSandbox) {
            Q_EMIT failed(path, Failure::NoSandbox);
            return;
        }
        // Parsing and cleaning the chapters can take a moment for a long book.
        auto *watcher = new QFutureWatcher<Epub::Book>(this);
        connect(watcher, &QFutureWatcher<Epub::Book>::finished, this, [this, watcher, path, generation] {
            watcher->deleteLater();
            if (generation != m_generation) {
                return; // another book meanwhile
            }
            const Epub::Book book = watcher->result();
            if (book.ok) {
                Q_EMIT finished(path, book);
            } else {
                Q_EMIT failed(path, Failure::Unreadable);
            }
        });
        watcher->setFuture(QtConcurrent::run([output] {
            const Epub::Files files = Epub::parseHelperOutput(output);
            if (!files.message.isEmpty()) {
                qCInfo(lcBook) << files.status << files.message;
            }
            return Epub::build(files);
        }));
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, path](QProcess::ProcessError error) {
        if (process == m_process && error == QProcess::FailedToStart) {
            stop();
            Q_EMIT failed(path, Failure::Unreadable);
        }
    });
    process->start(sandbox,
                   QStringList{u"--read"_s, path, u"--read"_s, helper, u"--"_s, helper, u"--files"_s, path, QString::number(FileLimit), QString::number(TotalLimit)}
                       + Epub::wantedSuffixes());
    m_timeout.start();
}

void BookReader::cancel()
{
    stop();
    ++m_generation;
}

void BookReader::stop()
{
    m_timeout.stop();
    if (QProcess *process = m_process) {
        m_process = nullptr;
        if (process->state() != QProcess::NotRunning && process->processId() > 0) {
            ::kill(-pid_t(process->processId()), SIGKILL); // the sandbox's own process group
            process->waitForFinished(1000);
        }
        process->deleteLater();
    }
}
