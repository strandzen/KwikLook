// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QUrl>

class QProcess;

/*
 * Office documents (DOCX, ODT, XLSX, ...) as PDFs for the PDF view: LibreOffice
 * converts them in the background, inside kwiklook-sandbox (no network, no access
 * to the user's files beyond the document), with a fresh profile each time so the
 * user's LibreOffice is untouched. Results are cached by file, size and date in
 * ~/.cache/kwiklook/office, at most CacheLimit bytes (least recently used go first).
 *
 * One conversion at a time: a new request (the next file) stops the running one.
 */
class OfficeConverter : public QObject
{
    Q_OBJECT

public:
    enum class Failure {
        NoSandbox, // the kernel can't lock LibreOffice in: never run unsandboxed
        Failed, // LibreOffice ended without a PDF (damaged file, timeout, ...)
    };
    Q_ENUM(Failure)

    // Converted PDFs kept: about 200 documents of 1 MB.
    static constexpr qint64 CacheLimit = 200 * 1024 * 1024;
    static constexpr int TimeoutMs = 60 * 1000;

    explicit OfficeConverter(QObject *parent = nullptr);
    ~OfficeConverter() override;

    // LibreOffice (soffice) is installed and the sandbox helper is there.
    static bool available();

    // Starts converting `path` (a local file) unless it's cached; emits converted() or
    // failed() for it, possibly before returning. Stops a running conversion.
    void convert(const QString &path);
    // Stops the running conversion, if any (no signal).
    void cancel();

    // Where results go, and how much is kept (tests change both).
    void setCacheDir(const QString &dir);
    QString cacheDir() const;
    void setCacheLimit(qint64 bytes);
    // The scratch folder of the running conversion (empty when none).
    QString workDir() const;

Q_SIGNALS:
    void converted(const QString &path, const QUrl &pdf);
    void failed(const QString &path, OfficeConverter::Failure failure);

private:
    QString cachedPdf(const QString &path) const;
    void finish(int exitCode, bool timedOut);
    void prune();

    QString m_cacheDir;
    qint64 m_cacheLimit = CacheLimit;
    QPointer<QProcess> m_process;
    QString m_path; // what m_process converts
    QString m_workDir; // its scratch folder (profile, temp, output)
    QTimer m_timeout;
};
