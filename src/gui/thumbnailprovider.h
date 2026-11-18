// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QQuickAsyncImageProvider>

/*
 * File thumbnails from KDE's thumbnailers (KIO::PreviewJob, as in Dolphin) for QML:
 *
 *   Image { source: "image://thumbnail/" + encodeURIComponent(mimeType) + "/" + encodeURIComponent(url) }
 *
 * The MIME type is passed in so KIO doesn't sniff the file on the main thread.
 * Gives a 1×1 transparent image when no thumbnailer handles the file (check
 * implicitWidth > 1), so that common case logs no warning.
 */
class ThumbnailProvider : public QQuickAsyncImageProvider
{
public:
    static constexpr auto Name = "thumbnail";
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
};
