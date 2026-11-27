// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "thumbnailprovider.h"

#include <KConfig>
#include <KConfigGroup>
#include <KFileItem>
#include <KIO/PreviewJob>

#include <QCoreApplication>
#include <QStandardPaths>

namespace
{
// The thumbnailers Dolphin uses (its “Previews” settings), so a file looks as it does
// there. Without a list KIO reads kdeglobals [PreviewSettings] Plugins, usually unset,
// and then makes thumbnails for pictures and folders only: no audio covers.
QStringList thumbnailPlugins()
{
    const KConfig dolphin(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/dolphinrc"), KConfig::SimpleConfig);
    return dolphin.group(QStringLiteral("PreviewSettings")).readEntry("Plugins", KIO::PreviewJob::defaultPlugins());
}
} // namespace

/*
 * Threads: Qt asks for a ThumbnailResponse on its image loader thread, which also
 * cancels and deletes it there. KIO jobs must run on the main thread. So the
 * response stays on the loader thread, and a ThumbnailRelay on the main thread runs
 * the job, sends the image back through a queued signal and deletes itself. The
 * response never touches the relay after creating it: if the response is gone by
 * then, Qt drops the queued signal. (Moving the response to the main thread
 * crashed when switching files quickly: both threads handled it.)
 */
class ThumbnailRelay : public QObject
{
    Q_OBJECT
public:
    void start(const QUrl &url, const QString &mimeType, const QSize &size)
    {
        const QStringList plugins = thumbnailPlugins();
        auto *job = KIO::filePreview(KFileItemList{KFileItem(url, mimeType)}, size, &plugins);
        job->setScaleType(KIO::PreviewJob::ScaledAndCached);
        connect(job, &KIO::PreviewJob::gotPreview, this, [this](const KFileItem &, const QPixmap &preview) {
            Q_EMIT done(preview.toImage());
        });
        connect(job, &KJob::result, this, [this] {
            // After gotPreview (or failed): no image means no thumbnailer for the type.
            Q_EMIT done(QImage());
            deleteLater();
        });
    }

Q_SIGNALS:
    void done(const QImage &image);
};

namespace
{
class ThumbnailResponse : public QQuickImageResponse
{
public:
    ThumbnailResponse(const QUrl &url, const QString &mimeType, const QSize &size)
    {
        auto *relay = new ThumbnailRelay;
        relay->moveToThread(QCoreApplication::instance()->thread());
        // Queued: runs on this (the loader) thread, only while this response exists.
        connect(relay, &ThumbnailRelay::done, this, &ThumbnailResponse::finish);
        QMetaObject::invokeMethod(
            relay,
            [relay, url, mimeType, size] {
                relay->start(url, mimeType, size);
            },
            Qt::QueuedConnection);
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    void cancel() override
    {
        // The job finishes on its own (thumbnails are quick); its result is ignored.
        finish(QImage());
    }

private:
    void finish(const QImage &image)
    {
        if (m_finished) {
            return; // the first answer counts (a thumbnail, then the job's end)
        }
        m_finished = true;
        if (image.isNull()) {
            // Common (no thumbnailer for the type), so not an error: an error would
            // log a QML warning per file. 1×1 transparent means “no thumbnail”.
            m_image = QImage(1, 1, QImage::Format_ARGB32_Premultiplied);
            m_image.fill(Qt::transparent);
        } else {
            m_image = image;
        }
        Q_EMIT finished();
    }

    QImage m_image;
    bool m_finished = false;
};
} // namespace

QQuickImageResponse *ThumbnailProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    // id = <MIME type>/<URL>, each percent-encoded.
    const qsizetype slash = id.indexOf(QLatin1Char('/'));
    const QString mimeType = QUrl::fromPercentEncoding(id.left(slash).toUtf8());
    const QUrl url(QUrl::fromPercentEncoding(id.mid(slash + 1).toUtf8()));
    const QSize size = requestedSize.isValid() ? requestedSize : QSize(512, 512);
    return new ThumbnailResponse(url, mimeType, size);
}

#include "thumbnailprovider.moc"
