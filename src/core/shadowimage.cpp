// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "shadowimage.h"

#include <QPainter>

namespace
{
// Three box blurs of the alpha channel approximate a Gaussian blur.
void boxBlurAlpha(QImage &img, int radius)
{
    if (radius < 1) {
        return;
    }
    const int w = img.width();
    const int h = img.height();
    QList<int> line(qMax(w, h));
    const int window = 2 * radius + 1;
    for (int pass = 0; pass < 3; ++pass) {
        for (int y = 0; y < h; ++y) {
            uchar *row = img.scanLine(y);
            for (int x = 0; x < w; ++x) {
                line[x] = row[x];
            }
            int sum = 0;
            for (int x = -radius; x <= radius; ++x) {
                sum += line[qBound(0, x, w - 1)];
            }
            for (int x = 0; x < w; ++x) {
                row[x] = uchar(sum / window);
                sum += line[qMin(x + radius + 1, w - 1)] - line[qMax(x - radius, 0)];
            }
        }
        for (int x = 0; x < w; ++x) {
            for (int y = 0; y < h; ++y) {
                line[y] = img.scanLine(y)[x];
            }
            int sum = 0;
            for (int y = -radius; y <= radius; ++y) {
                sum += line[qBound(0, y, h - 1)];
            }
            for (int y = 0; y < h; ++y) {
                img.scanLine(y)[x] = uchar(sum / window);
                sum += line[qMin(y + radius + 1, h - 1)] - line[qMax(y - radius, 0)];
            }
        }
    }
}
} // namespace

ShadowImage renderShadow(const QList<ShadowLayer> &layers, int cornerRadius, const QColor &color)
{
    ShadowImage result;
    if (layers.isEmpty()) {
        return result;
    }
    int padTop = 0, padSide = 0, padBottom = 0, maxSpread = 0;
    for (const ShadowLayer &l : layers) {
        const int spread = l.blur * 3; // three box passes reach 3 * radius
        padSide = qMax(padSide, spread);
        padTop = qMax(padTop, spread - l.offsetY);
        padBottom = qMax(padBottom, spread + l.offsetY);
        maxSpread = qMax(maxSpread, spread);
    }
    const int inner = cornerRadius + maxSpread;
    const QSize box(2 * inner + 1, 2 * inner + 1);
    const QSize size(padSide + box.width() + padSide, padTop + box.height() + padBottom);
    result.windowRect = QRect(QPoint(padSide, padTop), box);

    QImage alpha(size, QImage::Format_Alpha8);
    result.image = QImage(size, QImage::Format_ARGB32_Premultiplied);
    result.image.fill(Qt::transparent);
    QColor opaque = color;
    opaque.setAlpha(255);
    for (const ShadowLayer &l : layers) {
        alpha.fill(0);
        QPainter p(&alpha);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::black);
        p.drawRoundedRect(result.windowRect.translated(0, l.offsetY), cornerRadius, cornerRadius);
        p.end();
        boxBlurAlpha(alpha, l.blur);
        QImage layer(size, QImage::Format_ARGB32_Premultiplied);
        layer.fill(opaque);
        layer.setAlphaChannel(alpha);
        QPainter sp(&result.image);
        sp.setOpacity(l.opacity);
        sp.drawImage(0, 0, layer);
    }
    {
        QPainter p(&result.image); // no shadow under the window itself
        p.setRenderHint(QPainter::Antialiasing);
        p.setCompositionMode(QPainter::CompositionMode_Clear);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::black);
        p.drawRoundedRect(result.windowRect, cornerRadius, cornerRadius);
    }

    const int w = size.width();
    const int h = size.height();
    const int cornerW = padSide + inner;
    const int topH = padTop + inner;
    const int bottomH = padBottom + inner;
    const int midX = padSide + inner;
    const int midY = padTop + inner;
    result.padding = QMargins(padSide, padTop, padSide, padBottom);
    result.topLeft = QRect(0, 0, cornerW, topH);
    result.top = QRect(midX, 0, 1, padTop);
    result.topRight = QRect(w - cornerW, 0, cornerW, topH);
    result.right = QRect(w - padSide, midY, padSide, 1);
    result.bottomRight = QRect(w - cornerW, h - bottomH, cornerW, bottomH);
    result.bottom = QRect(midX, h - padBottom, 1, padBottom);
    result.bottomLeft = QRect(0, h - bottomH, cornerW, bottomH);
    result.left = QRect(0, midY, padSide, 1);
    return result;
}
