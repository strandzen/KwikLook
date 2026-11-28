// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>

// Shared by the smoke tests (tst_qmlsmoke_*.cpp): the preview window as the app
// loads it, offscreen, with the QML module linked in; helpers to find items and
// make test files.
#pragma once

#include "gui/autostart.h"
#include "gui/filemenus.h"
#include "gui/offlinenetwork.h"
#include "gui/sandbox.h"
#include "gui/thumbnailprovider.h"
#include "gui/viewer.h"
#include "kwiklooksettings.h"

#include <KLocalizedQmlContext>

#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFontMetricsF>
#include <QPointingDevice>
#include <QWheelEvent>
#include <QPainter>
#include <QPdfWriter>
#include <QQuickItem>
#include <QQuickWindow>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QTemporaryDir>
#include <QQuickTextDocument>
#include <QTextDocument>
#include <QTextBlock>
#include <QTemporaryFile>
#include <QStandardPaths>
#include <KPluginMetaData>
#include <QTest>

#include <KConfig>
#include <KConfigGroup>
#include <KSharedConfig>
#include <QDirIterator>
#include <KService>
#include <QMenu>
#include <QRawFont>
#include <QFontDatabase>
#include <QSignalSpy>
#include <QFileInfo>
#include <QProcess>
#include <QScopeGuard>
#include <QTcpServer>

#include <algorithm>
#include <memory>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(kwiklookPlugin)

using namespace Qt::StringLiterals;

// A 4×4 GIF with two frames (red, blue), made with ImageMagick: Qt can't write animated GIFs.
inline const QByteArray TwoFrameGif = QByteArray::fromRawData("\x47\x49\x46\x38\x39\x61\x04\x00\x04\x00\xf0\x00\x00\xff\x00\x00\x00\x00\x00\x21\xff\x0b\x4e\x45\x54\x53\x43\x41\x50\x45\x32\x2e\x30\x03\x01\x00\x00\x00\x21\xf9\x04\x00\x00\x00\x00\x00\x2c\x00\x00\x00\x00\x04\x00\x04\x00\x00\x02\x04\x84\x8f\x09\x05\x00\x21\xf9\x04\x00\x00\x00\x00\x00\x2c\x00\x00\x00\x00\x04\x00\x04\x00\x80\x00\x00\xff\x00\x00\x00\x02\x04\x84\x8f\x09\x05\x00\x3b", 95);

// The preview window as the app loads it, collecting QML warnings.
struct MainWindow {
    QQmlApplicationEngine engine;
    QList<QQmlError> errors;

    MainWindow()
    {
        KLocalization::setupLocalizedContext(&engine);
        engine.addImageProvider(QLatin1String(ThumbnailProvider::Name), new ThumbnailProvider);
        engine.setNetworkAccessManagerFactory(OfflineNetworkFactory::instance());
        QObject::connect(&engine, &QQmlApplicationEngine::warnings, &engine, [this](const QList<QQmlError> &w) {
            errors << w;
        });
        engine.loadFromModule("kwiklook", "Main");
    }
    QObject *root() const
    {
        return engine.rootObjects().value(0);
    }
    QQuickWindow *window() const
    {
        return qobject_cast<QQuickWindow *>(root());
    }
    QString warnings() const
    {
        QStringList lines;
        for (const QQmlError &e : errors) {
            lines << e.toString();
        }
        return lines.join(u'\n');
    }
};

// Items a Repeater or Loader made aren't QObject children of the window: walk the items.
// A three-page PDF with bookmarks (QPdfWriter writes none): “Chapter 1” (page 1, top)
// with “Section 1.1” (page 1, 400 points down), “Chapter 2” (page 3, top).
inline void writeBookmarkedPdf(const QString &path)
{
    const QList<QByteArray> objects{
        "<< /Type /Catalog /Pages 2 0 R /Outlines 6 0 R >>",
        "<< /Type /Pages /Kids [3 0 R 4 0 R 5 0 R] /Count 3 >>",
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
        "<< /Type /Outlines /First 7 0 R /Last 9 0 R /Count 3 >>",
        "<< /Title (Chapter 1) /Parent 6 0 R /Next 9 0 R /First 8 0 R /Last 8 0 R /Count 1 /Dest [3 0 R /XYZ 0 792 0] >>",
        "<< /Title (Section 1.1) /Parent 7 0 R /Dest [3 0 R /XYZ 0 392 0] >>",
        "<< /Title (Chapter 2) /Parent 6 0 R /Prev 7 0 R /Dest [5 0 R /XYZ 0 792 0] >>",
    };
    QByteArray data = "%PDF-1.4\n";
    QList<qsizetype> offsets;
    for (qsizetype i = 0; i < objects.size(); ++i) {
        offsets << data.size();
        data += QByteArray::number(i + 1) + " 0 obj\n" + objects.at(i) + "\nendobj\n";
    }
    const qsizetype xref = data.size();
    data += "xref\n0 " + QByteArray::number(objects.size() + 1) + "\n0000000000 65535 f \n";
    for (const qsizetype offset : offsets) {
        data += QByteArray::number(offset).rightJustified(10, '0') + " 00000 n \n";
    }
    data += "trailer\n<< /Size " + QByteArray::number(objects.size() + 1) + " /Root 1 0 R >>\nstartxref\n" + QByteArray::number(xref) + "\n%%EOF\n";
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(data);
    }
}

// Items below `root` with this object name, Repeater and ListView rows included.
inline QList<QQuickItem *> itemsNamed(QQuickItem *root, const QString &name)
{
    QList<QQuickItem *> found;
    QList<QQuickItem *> todo{root};
    while (!todo.isEmpty()) {
        QQuickItem *item = todo.takeFirst();
        if (item->objectName() == name && item->isVisible()) {
            found << item;
        }
        todo << item->childItems();
    }
    return found;
}

inline QQuickItem *findItem(QQuickWindow *window, const QString &name)
{
    QList<QQuickItem *> todo{window->contentItem()};
    while (!todo.isEmpty()) {
        QQuickItem *item = todo.takeFirst();
        if (item->objectName() == name) {
            return item;
        }
        todo << item->childItems();
    }
    return nullptr;
}

// Every smoke test class starts so (initTestCase / init): remembered state goes to a
// test folder; without the window (a missing QML module, e.g. org.kde.desktop from
// qqc2-desktop-style) every test would fail or crash, so a class stops at once and says why.
inline QString smokeSetUp()
{
    QStandardPaths::setTestModeEnabled(true);
    MainWindow main;
    return main.root() ? QString() : u"the window didn't load: "_s + main.warnings();
}

// Before each test: the viewer is shared by all tests of a class. A window closed at
// the end of one test leaves the app without focus, and “close when you switch” would
// then close the next test's preview 150 ms later. Not under test here.
inline void smokeInit()
{
    KwikLookSettings::self()->setCloseOnFocusLoss(false);
}
