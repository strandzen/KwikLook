// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
// File details and the choice of view, on scratch files.
#include "fileinfo.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

class TstFileInfo : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void localFile()
    {
        QTemporaryDir dir;
        QFile f(dir.filePath(u"notes.txt"_s));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("hello\n");
        f.close();
        const FileInfo info = FileInfo::read(QUrl::fromLocalFile(f.fileName()));
        QVERIFY(info.known);
        QVERIFY(info.exists);
        QVERIFY(!info.isDir);
        QCOMPARE(info.mimeType, u"text/plain"_s);
        QVERIFY(!info.typeName.isEmpty());
        QCOMPARE(info.iconName, u"text-plain"_s);
        QCOMPARE(info.size, 6);
        QVERIFY(info.modified.isValid());
    }

    void typeFromContent()
    {
        // A PNG signature in a file without an extension.
        QTemporaryDir dir;
        QFile f(dir.filePath(u"picture"_s));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("\x89PNG\r\n\x1a\n\0\0\0\rIHDR", 16);
        f.close();
        QCOMPARE(FileInfo::read(QUrl::fromLocalFile(f.fileName())).mimeType, u"image/png"_s);
    }

    void folder()
    {
        QTemporaryDir dir;
        const FileInfo info = FileInfo::read(QUrl::fromLocalFile(dir.path()));
        QVERIFY(info.exists);
        QVERIFY(info.isDir);
        QCOMPARE(info.mimeType, u"inode/directory"_s);
        QCOMPARE(info.size, -1);
    }

    void missingFile()
    {
        QTemporaryDir dir;
        const FileInfo info = FileInfo::read(QUrl::fromLocalFile(dir.filePath(u"gone.pdf"_s)));
        QVERIFY(info.known);
        QVERIFY(!info.exists);
        QCOMPARE(info.mimeType, u"application/pdf"_s); // from the name
        QVERIFY(!info.modified.isValid());
    }

    void remoteUrlByName()
    {
        const FileInfo info = FileInfo::read(QUrl(u"sftp://host/dir/photo.jpg"_s));
        QVERIFY(info.exists);
        QCOMPARE(info.mimeType, u"image/jpeg"_s);
    }

    void everythingFallsBackForNow()
    {
        QCOMPARE(viewFor(FileInfo()), u"FallbackView"_s);
        QCOMPARE(viewFor(FileInfo::read(QUrl(u"file:///nope.png"_s))), u"FallbackView"_s);
    }
};

QTEST_GUILESS_MAIN(TstFileInfo)
#include "tst_fileinfo.moc"
