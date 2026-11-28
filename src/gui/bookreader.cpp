// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "bookreader.h"

#include "archivemodel.h"
#include "sandbox.h"

#include <QFutureWatcher>
#include <QLoggingCategory>
#include <QtConcurrentRun>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcBook, "kwiklook.book", QtInfoMsg)

BookReader::BookReader(QObject *parent)
    : QObject(parent)
    , m_run(new SandboxRun(TimeoutMs, this))
{
    connect(m_run, &SandboxRun::finished, this, [this](SandboxRun::End end, int, const QByteArray &output) {
        const QString path = m_path;
        switch (end) {
        case SandboxRun::End::NoSandbox:
            Q_EMIT failed(path, Failure::NoSandbox);
            break;
        case SandboxRun::End::TimedOut:
        case SandboxRun::End::Failed:
            Q_EMIT failed(path, Failure::Unreadable);
            break;
        case SandboxRun::End::Exited:
            build(path, output, m_generation); // the helper says how far it got in its output
            break;
        }
    });
}

BookReader::~BookReader()
{
    cancel();
}

void BookReader::read(const QString &path)
{
    cancel();
    m_path = path;
    const QString helper = ArchiveModel::helper();
    if (Sandbox::binary().isEmpty() || helper.isEmpty() || !Sandbox::supported()) {
        QMetaObject::invokeMethod(this, [this, path] { Q_EMIT failed(path, Failure::NoSandbox); }, Qt::QueuedConnection);
        return;
    }
    m_run->start(QStringList{u"--read"_s, path, u"--read"_s, helper, u"--"_s, helper, u"--files"_s, path, QString::number(FileLimit), QString::number(TotalLimit)}
                 + Epub::wantedSuffixes());
}

void BookReader::build(const QString &path, const QByteArray &output, int generation)
{
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
}

void BookReader::cancel()
{
    m_run->cancel();
    ++m_generation; // drops a book still being built
}
