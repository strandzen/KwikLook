// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "breezestyle.h"

#include <KConfig>
#include <KConfigGroup>

#include <QFileInfo>

using namespace Qt::StringLiterals;

namespace
{
BreezeStyle::ShadowSize parseSize(const QString &value, BreezeStyle::ShadowSize fallback)
{
    // KConfig stores enums by choice name; accept the index too.
    static const QStringList names = {u"ShadowNone"_s, u"ShadowSmall"_s, u"ShadowMedium"_s, u"ShadowLarge"_s, u"ShadowVeryLarge"_s};
    for (int i = 0; i < names.size(); ++i) {
        if (value.compare(names.at(i), Qt::CaseInsensitive) == 0) {
            return BreezeStyle::ShadowSize(i);
        }
    }
    bool ok = false;
    const int index = value.toInt(&ok);
    return ok && index >= 0 && index < names.size() ? BreezeStyle::ShadowSize(index) : fallback;
}
} // namespace

BreezeStyle BreezeStyle::load(const QString &breezercPath)
{
    BreezeStyle style;
    if (breezercPath.isEmpty() || !QFileInfo(breezercPath).isAbsolute() || !QFileInfo::exists(breezercPath)) {
        return style;
    }
    const KConfig config(breezercPath, KConfig::SimpleConfig);
    const KConfigGroup common = config.group(u"Common"_s);
    style.shadowSize = parseSize(common.readEntry("ShadowSize", QString()), style.shadowSize);
    style.shadowStrength = qBound(0, common.readEntry("ShadowStrength", style.shadowStrength), 255);
    style.shadowColor = common.readEntry("ShadowColor", style.shadowColor);
    style.outline = common.readEntry("OutlineEnabled", style.outline);
    style.roundedCorners = common.readEntry("RoundedCorners", style.roundedCorners);
    return style;
}

QList<ShadowLayer> BreezeStyle::shadowLayers() const
{
    // Fitted to Breeze windows measured in a nested session, each size within 0.02
    // of Breeze's shadow on all four sides.
    QList<ShadowLayer> layers;
    switch (shadowSize) {
    case ShadowSize::None:
        return {};
    case ShadowSize::Small:
        layers = {{4, 6, 0.60}, {1, 2, 0.25}};
        break;
    case ShadowSize::Medium:
        layers = {{6, 10, 0.75}, {9, 19, 0.20}};
        break;
    case ShadowSize::Large:
        layers = {{12, 20, 0.70}, {2, 11, 0.15}};
        break;
    case ShadowSize::VeryLarge:
        layers = {{16, 28, 0.60}, {6, 14, 0.15}};
        break;
    }
    if (shadowStrength == 0) {
        return {};
    }
    const qreal strength = shadowStrength / 255.0;
    for (ShadowLayer &l : layers) {
        l.opacity *= strength;
    }
    return layers;
}

int BreezeStyle::cornerRadius() const
{
    return roundedCorners ? CornerRadius : 0;
}
