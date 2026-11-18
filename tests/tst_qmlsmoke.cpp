// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "gui/thumbnailprovider.h"
#include "gui/viewer.h"

#include <KLocalizedQmlContext>

#include <QQmlApplicationEngine>
#include <QDir>
#include <QQuickItem>
#include <QImage>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QStandardPaths>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(kwiklookPlugin)

class TstQmlSmoke : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true); // remembered state goes to a test folder
    }

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

    void loadsFallbackViewForAFile()
    {
        QQmlApplicationEngine engine;
        KLocalization::setupLocalizedContext(&engine);
        engine.addImageProvider(QLatin1String(ThumbnailProvider::Name), new ThumbnailProvider);
        QList<QQmlError> errors;
        connect(&engine, &QQmlApplicationEngine::warnings, this, [&errors](const QList<QQmlError> &w) {
            errors << w;
        });
        engine.loadFromModule("kwiklook", "Main");
        QCOMPARE(engine.rootObjects().size(), 1);
        QTemporaryFile file(QDir::tempPath() + QStringLiteral("/kwiklook-XXXXXX.txt"));
        QVERIFY(file.open());
        file.write("hello");
        file.flush();
        Viewer::instance()->Open({file.fileName()}, 0, QString(), QString());
        QTRY_COMPARE(Viewer::instance()->view(), QStringLiteral("FallbackView"));
        QTRY_VERIFY(engine.rootObjects().constFirst()->findChild<QQuickItem *>(QStringLiteral("fallbackView")));
        QTest::qWait(200); // let the thumbnail request finish or fail
        for (const QQmlError &e : std::as_const(errors)) {
            qWarning().noquote() << e.toString();
        }
        QVERIFY(errors.isEmpty());
    }

    void loadsImageViewWithInfoPanel()
    {
        QQmlApplicationEngine engine;
        KLocalization::setupLocalizedContext(&engine);
        engine.addImageProvider(QLatin1String(ThumbnailProvider::Name), new ThumbnailProvider);
        QList<QQmlError> errors;
        connect(&engine, &QQmlApplicationEngine::warnings, this, [&errors](const QList<QQmlError> &w) {
            errors << w;
        });
        engine.loadFromModule("kwiklook", "Main");
        QCOMPARE(engine.rootObjects().size(), 1);
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("pic.png"));
        QImage img(40, 30, QImage::Format_RGB32);
        img.fill(Qt::green);
        QVERIFY(img.save(path));
        Viewer::instance()->setInfoOpen(true);
        Viewer::instance()->Open({path}, 0, QString(), QString());
        QObject *root = engine.rootObjects().constFirst();
        QTRY_VERIFY(root->findChild<QQuickItem *>(QStringLiteral("imageView")));
        QTRY_VERIFY(root->findChild<QQuickItem *>(QStringLiteral("infoPanel")));
        QTest::qWait(300);
        Viewer::instance()->setInfoOpen(false);
        for (const QQmlError &e : std::as_const(errors)) {
            qWarning().noquote() << e.toString();
        }
        QVERIFY(errors.isEmpty());
    }

};

QTEST_MAIN(TstQmlSmoke)
#include "tst_qmlsmoke.moc"
