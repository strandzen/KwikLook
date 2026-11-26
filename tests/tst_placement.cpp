// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
// The geometry of KwikLook's KWin script (data/kwin/kwiklook-placement.js), run in a
// QJSEngine: without KWin's `workspace` the script defines its functions and nothing else.
#include <QFile>
#include <QJSEngine>
#include <QTest>

using namespace Qt::StringLiterals;

class TstPlacement : public QObject
{
    Q_OBJECT

    QJSEngine m_js;

    QJSValue run(const QString &code)
    {
        const QJSValue result = m_js.evaluate(code);
        if (result.isError()) {
            qWarning() << result.toString();
        }
        return result;
    }

    // "1200x900" from {width: 1200, height: 900}, "10,20 300x200" with a position.
    QString text(const QString &code)
    {
        return run(u"(r => (r.x !== undefined ? r.x + ',' + r.y + ' ' : '') + r.width + 'x' + r.height)(%1)"_s.arg(code)).toString();
    }

private Q_SLOTS:
    void initTestCase()
    {
        QFile file(QStringLiteral(PLACEMENT_SCRIPT));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJSValue result = m_js.evaluate(QString::fromUtf8(file.readAll()), file.fileName());
        QVERIFY2(!result.isError(), qPrintable(result.toString()));
        QVERIFY(m_js.globalObject().property(u"fitSize"_s).isCallable());
        run(u"var area = {x: 0, y: 0, width: 1500, height: 950};"
            "var chrome = {width: 24, height: 70};"
            "var minimum = {width: 360, height: 200};"_s);
    }

    void smallPictureAtRealSize()
    {
        QCOMPARE(text(u"fitSize({picture: {width: 400, height: 300}, chrome, minimum}, area)"_s), u"424x370"_s);
    }

    void largePictureFitsKeepingItsShape()
    {
        // 80 % of 950 = 760 high: the picture gets 690, so 920 wide (4:3).
        QCOMPARE(text(u"fitSize({picture: {width: 4000, height: 3000}, chrome, minimum}, area)"_s), u"944x760"_s);
        // A panorama is limited by the width instead (1176 of 9000): 148 high, raised to the minimum.
        QCOMPARE(text(u"fitSize({picture: {width: 9000, height: 600}, chrome, minimum}, area)"_s), u"1200x200"_s);
    }

    void neverBelowTheMinimum()
    {
        QCOMPARE(text(u"fitSize({picture: {width: 16, height: 16}, chrome, minimum}, area)"_s), u"360x200"_s);
    }

    void centredInTheArea()
    {
        QCOMPARE(text(u"placeIn({width: 600, height: 400}, centreOf(area), area)"_s), u"450,275 600x400"_s);
    }

    void keptInsideTheArea()
    {
        // At the pointer near the bottom right corner: pushed back inside.
        QCOMPARE(text(u"placeIn({width: 600, height: 400}, {x: 1450, y: 900}, area)"_s), u"900,550 600x400"_s);
        // A second screen to the right, with a panel at its top.
        QCOMPARE(text(u"placeIn({width: 600, height: 400}, {x: 1600, y: 10}, {x: 1500, y: 40, width: 1920, height: 1040})"_s),
                 u"1500,40 600x400"_s);
    }

    void shrunkWhenLargerThanTheArea()
    {
        QCOMPARE(text(u"placeIn({width: 2000, height: 1200}, centreOf(area), area)"_s), u"0,0 1500x950"_s);
    }
};

QTEST_GUILESS_MAIN(TstPlacement)
#include "tst_placement.moc"
