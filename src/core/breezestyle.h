// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QColor>
#include <QList>
#include <QString>

// One layer of a window shadow: a copy of the window shape moved down by `offsetY`,
// blurred by three box blurs of radius `blur`, at `opacity`.
struct ShadowLayer {
    int offsetY = 0;
    int blur = 0;
    qreal opacity = 0;
    bool operator==(const ShadowLayer &) const = default;
};

/*
 * The parts of the user's Breeze window decoration settings (breezerc, [Common])
 * that KwikLook's frameless window copies, so it looks like other windows.
 * Breeze offers no API for this; the shadow values were measured from real Breeze
 * windows (Plasma 6.8, 2026-11-18) and fitted, see CLAUDE.md.
 */
struct BreezeStyle {
    enum class ShadowSize { None, Small, Medium, Large, VeryLarge };

    ShadowSize shadowSize = ShadowSize::Large;
    int shadowStrength = 255; // 0-255
    QColor shadowColor = Qt::black;
    bool outline = true;
    bool roundedCorners = true;

    // Corner radius of Breeze windows (measured, default font and scale).
    static constexpr int CornerRadius = 5;

    // Reads `breezercPath` (an absolute path); missing file or keys give Breeze's defaults.
    static BreezeStyle load(const QString &breezercPath);

    // The shadow for these settings, strength already applied. Empty for no shadow.
    QList<ShadowLayer> shadowLayers() const;
    int cornerRadius() const;


    bool operator==(const BreezeStyle &) const = default;
};
