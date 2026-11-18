// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
// File details and the choice of view, on scratch files.
#include "fileinfo.h"

#include <QFile>
#include <QImage>
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

    void pictureSizeAndView()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(u"pic.png"_s);
        QImage img(120, 80, QImage::Format_RGB32);
        img.fill(Qt::red);
        QVERIFY(img.save(path));
        const FileInfo info = FileInfo::read(QUrl::fromLocalFile(path));
        QCOMPARE(info.imageSize, QSize(120, 80));
        QCOMPARE(viewFor(info), u"ImageView"_s);
        for (const FileInfo::Detail &d : info.details) {
            QVERIFY2(!d.value.isEmpty(), qPrintable(d.label));
        }
        // Without details: no parsing.
        QVERIFY(!FileInfo::read(QUrl::fromLocalFile(path), false).imageSize.isValid());
    }

    void textAndPdfViews()
    {
        QTemporaryDir dir;
        QFile code(dir.filePath(u"main.cpp"_s));
        QVERIFY(code.open(QIODevice::WriteOnly));
        code.write("int main() {}\n");
        code.close();
        const FileInfo info = FileInfo::read(QUrl::fromLocalFile(code.fileName()));
        QVERIFY(info.isText);
        QCOMPARE(info.text, u"int main() {}\n"_s);
        QVERIFY(!info.textTruncated);
        QCOMPARE(viewFor(info), u"TextView"_s);

        QFile pdf(dir.filePath(u"doc.pdf"_s));
        QVERIFY(pdf.open(QIODevice::WriteOnly));
        pdf.write("%PDF-1.4\n%%EOF\n");
        pdf.close();
        QCOMPARE(viewFor(FileInfo::read(QUrl::fromLocalFile(pdf.fileName()), false)), u"PdfView"_s);
    }

    void longTextIsCutAtALineEnd()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(u"log.txt"_s);
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("first line\nsecond line\nthird ünïcödé line\n");
        f.close();
        bool truncated = false;
        QCOMPARE(readTextStart(path, 1000, &truncated), u"first line\nsecond line\nthird ünïcödé line\n"_s);
        QVERIFY(!truncated);
        QCOMPARE(readTextStart(path, 30, &truncated), u"first line\nsecond line\n"_s);
        QVERIFY(truncated);
        // Not UTF-8: read as Latin-1, not dropped.
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("caf\xe9\n");
        f.close();
        QCOMPARE(readTextStart(path, 1000, &truncated), u"café\n"_s);
    }

    void otherFilesFallBack()
    {
        QCOMPARE(viewFor(FileInfo()), u"FallbackView"_s);
        QCOMPARE(viewFor(FileInfo::read(QUrl(u"file:///nope.png"_s))), u"FallbackView"_s); // missing
        QTemporaryDir dir;
        QCOMPARE(viewFor(FileInfo::read(QUrl::fromLocalFile(dir.path()))), u"FallbackView"_s);
    }

    void ownerAndPermissions()
    {
        QTemporaryDir dir;
        QFile f(dir.filePath(u"mine.txt"_s));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup));
        const FileInfo info = FileInfo::read(QUrl::fromLocalFile(f.fileName()));
        QVERIFY(!info.owner.isEmpty());
        QCOMPARE(info.permissions & (QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::WriteGroup | QFile::ReadOther),
                 QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup);
    }
};

QTEST_GUILESS_MAIN(TstFileInfo)
#include "tst_fileinfo.moc"
