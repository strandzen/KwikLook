// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include "epub.h"

#include <QObject>
#include <QPointer>
#include <QTimer>

class QProcess;

// EPUB books for the preview: kwiklook-archive --files unpacks the book's text and
// pictures inside kwiklook-sandbox (nothing else is read, nothing written), then
// Epub::build makes the document off the main thread. One book at a time: a new
// read() or cancel() stops the one before.
class BookReader : public QObject
{
    Q_OBJECT

public:
    enum class Failure {
        NoSandbox, // this system can't lock the helper in: books aren't unpacked
        Unreadable, // not a book, or damaged
    };
    Q_ENUM(Failure)

    explicit BookReader(QObject *parent = nullptr);
    ~BookReader() override;

    static constexpr qint64 FileLimit = 16 * 1024 * 1024; // one file in the book, unpacked
    static constexpr qint64 TotalLimit = 96 * 1024 * 1024; // the files handed over in all
    static constexpr int TimeoutMs = 15000;

    void read(const QString &path);
    void cancel();

Q_SIGNALS:
    void finished(const QString &path, const Epub::Book &book);
    void failed(const QString &path, BookReader::Failure failure);

private:
    void stop();

    QPointer<QProcess> m_process;
    QString m_path;
    int m_generation = 0;
    QTimer m_timeout;
};
