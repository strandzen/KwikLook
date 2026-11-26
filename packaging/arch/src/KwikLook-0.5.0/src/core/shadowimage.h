// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include "breezestyle.h"

#include <QImage>
#include <QMargins>
#include <QRect>

/*
 * A window shadow as nine-patch tiles for the compositor (KWindowShadow).
 * `image` is the shadow of a small window-shaped box; the tiles are cut from it.
 * Corner tiles reach `inner` pixels into the window so the blur around a rounded
 * corner is complete; the window's own shape is cleared from the image.
 */
struct ShadowImage {
    QImage image; // empty: no shadow
    QMargins padding; // how far the shadow reaches outside the window
    QRect topLeft, top, topRight, right, bottomRight, bottom, bottomLeft, left;
    QRect windowRect; // the window box inside `image`
};

ShadowImage renderShadow(const QList<ShadowLayer> &layers, int cornerRadius, const QColor &color);
