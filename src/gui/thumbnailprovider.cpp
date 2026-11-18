// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "thumbnailprovider.h"

#include <KFileItem>
#include <KIO/PreviewJob>

#include <QCoreApplication>
#include <QPointer>

namespace
{
// Lives on the main thread (KIO jobs must), though Qt asks for it from its image loader thread.
class ThumbnailResponse : public QQuickImageResponse
{
public:
    ThumbnailResponse(const QUrl &url, const QString &mimeType, const QSize &size)
    {
        moveToThread(QCoreApplication::instance()->thread());
        QMetaObject::invokeMethod(
            this,
            [this, url, mimeType, size] {
                start(url, mimeType, size);
            },
            Qt::QueuedConnection);
    }

    ~ThumbnailResponse() override
    {
        if (m_job) {
            m_job->kill();
        }
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    QString errorString() const override
    {
        return m_error;
    }

    void cancel() override
    {
        if (m_job) {
            m_job->kill(); // quietly: no gotPreview/failed afterwards
        }
        m_error = QStringLiteral("cancelled");
        Q_EMIT finished();
    }

private:
    void start(const QUrl &url, const QString &mimeType, const QSize &size)
    {
        m_job = KIO::filePreview(KFileItemList{KFileItem(url, mimeType)}, size);
        m_job->setScaleType(KIO::PreviewJob::ScaledAndCached);
        connect(m_job, &KIO::PreviewJob::gotPreview, this, [this](const KFileItem &, const QPixmap &preview) {
            m_image = preview.toImage();
            Q_EMIT finished();
        });
        connect(m_job, &KIO::PreviewJob::failed, this, [this] {
            // Common (no thumbnailer for the type), so not an error: an error would log
            // a QML warning per file. 1×1 transparent means “no thumbnail”.
            m_image = QImage(1, 1, QImage::Format_ARGB32_Premultiplied);
            m_image.fill(Qt::transparent);
            Q_EMIT finished();
        });
    }

    QPointer<KIO::PreviewJob> m_job;
    QImage m_image;
    QString m_error;
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
