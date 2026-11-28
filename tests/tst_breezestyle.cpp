// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
// Reading the Breeze settings, and the shadow made from them.
#include "breezestyle.h"
#include "shadowimage.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

class TstBreezeStyle : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    QString writeRc(const QByteArray &contents)
    {
        const QString path = m_dir.filePath(u"breezerc"_s); // absolute, never the user's
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            qFatal("cannot write %s", qPrintable(path));
        }
        f.write(contents);
        return path;
    }

    // Darkening `d` pixels outside the window edge: shadow alpha at that pixel.
    static qreal below(const ShadowImage &s, int d)
    {
        return qAlpha(s.image.pixel(s.windowRect.center().x(), s.windowRect.bottom() + d)) / 255.0;
    }
    static qreal above(const ShadowImage &s, int d)
    {
        return qAlpha(s.image.pixel(s.windowRect.center().x(), s.windowRect.top() - d)) / 255.0;
    }
    static qreal side(const ShadowImage &s, int d)
    {
        return qAlpha(s.image.pixel(s.windowRect.left() - d, s.windowRect.center().y())) / 255.0;
    }

private Q_SLOTS:
    void missingFileGivesBreezeDefaults()
    {
        const BreezeStyle s = BreezeStyle::load(m_dir.filePath(u"nothing-here"_s));
        QCOMPARE(s, BreezeStyle());
        QCOMPARE(s.shadowSize, BreezeStyle::ShadowSize::Large);
        QCOMPARE(s.cornerRadius(), BreezeStyle::CornerRadius);
        QVERIFY(s.outline);
    }

    void relativePathIsNeverRead()
    {
        // A bare name would resolve into the user's config folder (CLAUDE.md).
        QCOMPARE(BreezeStyle::load(u"breezerc"_s), BreezeStyle());
    }

    void readsCommonGroup()
    {
        const BreezeStyle s = BreezeStyle::load(writeRc(
            "[Common]\nShadowSize=ShadowSmall\nShadowStrength=128\nShadowColor=255,0,0\nOutlineEnabled=false\nRoundedCorners=false\n"));
        QCOMPARE(s.shadowSize, BreezeStyle::ShadowSize::Small);
        QCOMPARE(s.shadowStrength, 128);
        QCOMPARE(s.shadowColor, QColor(255, 0, 0));
        QVERIFY(!s.outline);
        QCOMPARE(s.cornerRadius(), 0);
    }

    void shadowSizeAsNumberAndJunk()
    {
        QCOMPARE(BreezeStyle::load(writeRc("[Common]\nShadowSize=4\n")).shadowSize, BreezeStyle::ShadowSize::VeryLarge);
        QCOMPARE(BreezeStyle::load(writeRc("[Common]\nShadowSize=Huge\n")).shadowSize, BreezeStyle::ShadowSize::Large);
        QCOMPARE(BreezeStyle::load(writeRc("[Common]\nShadowStrength=900\n")).shadowStrength, 255);
    }

    void strengthScalesOpacity()
    {
        BreezeStyle s;
        const auto full = s.shadowLayers();
        s.shadowStrength = 128;
        const auto half = s.shadowLayers();
        QCOMPARE(half.size(), full.size());
        QVERIFY(qAbs(half.first().opacity - full.first().opacity * 128 / 255) < 1e-9);
        s.shadowStrength = 0;
        QVERIFY(s.shadowLayers().isEmpty());
        s.shadowStrength = 255;
        s.shadowSize = BreezeStyle::ShadowSize::None;
        QVERIFY(s.shadowLayers().isEmpty());
        QVERIFY(renderShadow({}, 5, Qt::black).image.isNull());
    }

    void largeShadowMatchesMeasuredBreeze()
    {
        // Darkening next to a real Breeze window, default settings, measured from
        // nested-session screenshots on 2026-11-18.
        const BreezeStyle s;
        const ShadowImage img = renderShadow(s.shadowLayers(), s.cornerRadius(), s.shadowColor);
        const struct {
            qreal (*at)(const ShadowImage &, int);
            int d;
            qreal breeze;
        } samples[] = {
            {below, 1, 0.54}, {below, 11, 0.39}, {below, 21, 0.24}, {below, 41, 0.06},
            {above, 1, 0.24}, {above, 11, 0.12}, {side, 1, 0.39}, {side, 11, 0.24}, {side, 21, 0.13},
        };
        for (const auto &x : samples) {
            const qreal ours = x.at(img, x.d);
            QVERIFY2(qAbs(ours - x.breeze) < 0.03, qPrintable(u"at %1 px: %2, Breeze %3"_s.arg(x.d).arg(ours).arg(x.breeze)));
        }
    }

    void tilesFitTheImage()
    {
        const BreezeStyle s;
        const ShadowImage img = renderShadow(s.shadowLayers(), s.cornerRadius(), s.shadowColor);
        const QRect bounds = img.image.rect();
        for (const QRect &r : {img.topLeft, img.top, img.topRight, img.right, img.bottomRight, img.bottom, img.bottomLeft, img.left}) {
            QVERIFY(!r.isEmpty());
            QVERIFY(bounds.contains(r));
        }
        // Nothing under the window itself.
        QCOMPARE(qAlpha(img.image.pixel(img.windowRect.center())), 0);
    }
};

QTEST_GUILESS_MAIN(TstBreezeStyle)
#include "tst_breezestyle.moc"
