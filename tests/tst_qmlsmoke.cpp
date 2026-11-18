// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include <KLocalizedQmlContext>

#include <QQmlApplicationEngine>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(kwiklookPlugin)

class TstQmlSmoke : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void loadsMainWindow()
    {
        QQmlApplicationEngine engine;
        KLocalization::setupLocalizedContext(&engine);
        QList<QQmlError> errors;
        connect(&engine, &QQmlApplicationEngine::warnings, this, [&errors](const QList<QQmlError> &w) {
            errors << w;
        });
        engine.loadFromModule("kwiklook", "Main");
        QCOMPARE(engine.rootObjects().size(), 1);
        for (const QQmlError &e : std::as_const(errors)) {
            qWarning().noquote() << e.toString();
        }
        QVERIFY(errors.isEmpty());
    }
};

QTEST_MAIN(TstQmlSmoke)
#include "tst_qmlsmoke.moc"
