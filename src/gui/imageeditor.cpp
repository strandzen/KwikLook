// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "imageeditor.h"
#include "imagemetadata.h"

#include <KFileUtils>
#include <KIO/CopyJob>
#include <KLocalizedString>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QImageReader>
#include <QImageWriter>
#include <QLoggingCategory>
#include <QMimeDatabase>
#include <QTransform>
#include <QtConcurrentRun>

#include <cmath>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcImageEditor, "kwiklook.imageeditor")

namespace
{
// The edited picture, written under a hidden name next to the original; empty on failure.
QString writeEdited(const QString &source, const QRectF &crop, int turn)
{
    QImageReader reader(source);
    reader.setAutoTransform(true); // as shown: EXIF orientation applied
    QImage image = reader.read();
    if (image.isNull()) {
        qCWarning(lcImageEditor) << "could not read" << source << reader.errorString();
        return QString();
    }
    if (turn % 360 != 0) {
        image = image.transformed(QTransform().rotate(-turn)); // counterclockwise
    }
    const QRect area = QRect(int(std::lround(crop.x() * image.width())), int(std::lround(crop.y() * image.height())),
                             int(std::lround(crop.width() * image.width())), int(std::lround(crop.height() * image.height())))
                           .intersected(image.rect());
    if (area.isEmpty()) {
        return QString();
    }
    image = image.copy(area);

    const QString suffix = ImageEditor::outputSuffix(source);
    const QFileInfo file(source);
    const QString written = file.absoluteDir().filePath(u".%1.kwiklook-edit.%2"_s.arg(file.completeBaseName(), suffix));
    QImageWriter writer(written, suffix.toLatin1());
    if (writer.supportsOption(QImageIOHandler::Quality)) {
        writer.setQuality(95);
    }
    if (!writer.write(image)) {
        qCWarning(lcImageEditor) << "could not write" << written << writer.errorString();
        QFile::remove(written);
        return QString();
    }
    copyImageMetadata(source, written, image.size()); // the picture is fine without, too
    return written;
}

// A free name next to `path` for a file of type `suffix`: `base` itself, else as
// KDE suggests (“photo (edited) (1).jpg”).
QString freeName(const QString &path, const QString &base, const QString &suffix)
{
    const QDir dir = QFileInfo(path).absoluteDir();
    const QString name = base + u'.' + suffix;
    return dir.filePath(dir.exists(name) ? KFileUtils::suggestName(QUrl::fromLocalFile(dir.path()), name) : name);
}
} // namespace

ImageEditor::ImageEditor(QObject *parent)
    : QObject(parent)
    , m_trash([](const QString &path, QObject *context, std::function<void(const QString &)> done) {
        auto *job = KIO::trash(QUrl::fromLocalFile(path), KIO::HideProgressInfo);
        QObject::connect(job, &KJob::result, context, [job, done] {
            done(job->error() ? job->errorString() : QString());
        });
    })
{
}

void ImageEditor::setTrashFunction(TrashFunction trash)
{
    m_trash = std::move(trash);
}

bool ImageEditor::busy() const
{
    return m_busy;
}

QString ImageEditor::outputSuffix(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    const QByteArray format = suffix == u"jpeg"_s || suffix == u"jpe"_s ? QByteArray("jpg") : suffix.toLatin1();
    if (QImageWriter::supportedImageFormats().contains(format)) {
        return QString::fromLatin1(format);
    }
    // Camera raw files: a photo, so JPEG; the rest (still GIF, ...) lossless.
    const QMimeType mime = QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchExtension);
    return mime.inherits(u"image/x-dcraw"_s) ? u"jpg"_s : u"png"_s;
}

void ImageEditor::save(const QUrl &source, const QRectF &crop, int turn, bool replace)
{
    if (m_busy || !source.isLocalFile()) {
        return;
    }
    m_busy = true;
    Q_EMIT busyChanged();
    const QString path = source.toLocalFile();
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, path, replace] {
        watcher->deleteLater();
        const QString written = watcher->result();
        if (written.isEmpty()) {
            fail(QString(), i18nc("@info", "Could not save the picture."));
            return;
        }
        finish(path, written, replace);
    });
    watcher->setFuture(QtConcurrent::run(writeEdited, path, crop, turn));
}

void ImageEditor::finish(const QString &source, const QString &written, bool replace)
{
    const QFileInfo original(source);
    const QString suffix = QFileInfo(written).suffix();
    if (!replace) {
        const QString copy = freeName(source, i18nc("file name of an edited copy, e.g. photo (edited).jpg", "%1 (edited)", original.completeBaseName()), suffix);
        if (!QFile::rename(written, copy)) {
            fail(written, i18nc("@info", "Could not save the picture next to the original."));
            return;
        }
        m_busy = false;
        Q_EMIT busyChanged();
        Q_EMIT saved(QUrl::fromLocalFile(copy), false);
        return;
    }
    // In place: the original to the Trash first, so it can be restored; then the new
    // picture takes its name (or, with another type, its name with the new suffix).
    m_trash(source, this, [this, source, written, suffix](const QString &error) {
        if (!error.isEmpty()) {
            qCWarning(lcImageEditor) << "could not move to the Trash:" << error;
            fail(written, i18nc("@info", "The original could not be moved to the Trash, so it was kept and nothing was replaced."));
            return;
        }
        const QFileInfo original(source);
        const QString target = original.suffix().compare(suffix, Qt::CaseInsensitive) == 0 || (suffix == u"jpg"_s && original.suffix().toLower() == u"jpeg"_s)
            ? source
            : freeName(source, original.completeBaseName(), suffix);
        if (!QFile::rename(written, target)) {
            fail(written, i18nc("@info", "Could not save the picture. The original is in the Trash."));
            return;
        }
        m_busy = false;
        Q_EMIT busyChanged();
        Q_EMIT saved(QUrl::fromLocalFile(target), true);
    });
}

void ImageEditor::fail(const QString &written, const QString &message)
{
    if (!written.isEmpty()) {
        QFile::remove(written); // our own hidden file, never the user's
    }
    m_busy = false;
    Q_EMIT busyChanged();
    Q_EMIT failed(message);
}
