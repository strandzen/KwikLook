// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
// File details and the choice of view, on scratch files.
#include "fileinfo.h"

#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QTest>

using namespace Qt::StringLiterals;

// A 4×4 GIF with two frames (red, blue), made with ImageMagick: Qt can't write animated GIFs.
static const QByteArray TwoFrameGif = QByteArray::fromRawData("\x47\x49\x46\x38\x39\x61\x04\x00\x04\x00\xf0\x00\x00\xff\x00\x00\x00\x00\x00\x21\xff\x0b\x4e\x45\x54\x53\x43\x41\x50\x45\x32\x2e\x30\x03\x01\x00\x00\x00\x21\xf9\x04\x00\x00\x00\x00\x00\x2c\x00\x00\x00\x00\x04\x00\x04\x00\x00\x02\x04\x84\x8f\x09\x05\x00\x21\xf9\x04\x00\x00\x00\x00\x00\x2c\x00\x00\x00\x00\x04\x00\x04\x00\x80\x00\x00\xff\x00\x00\x00\x02\x04\x84\x8f\x09\x05\x00\x3b", 95);

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
        QCOMPARE(viewFor(FileInfo::read(QUrl::fromLocalFile(pdf.fileName()))), u"PdfView"_s);
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

    void textWithAByteOrderMark()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(u"utf16.txt"_s);
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        const QString text = u"Hej æøå\nanden linje\n"_s;
        f.write("\xff\xfe"); // UTF-16 little endian
        f.write(reinterpret_cast<const char *>(text.utf16()), text.size() * 2);
        f.close();
        bool truncated = false;
        QCOMPARE(readTextStart(path, 1000, &truncated), text);
        QVERIFY(!truncated);
        // Cut inside the second line: ends after the first.
        QCOMPARE(readTextStart(path, 2 + 2 * 10, &truncated), u"Hej æøå\n"_s);
        QVERIFY(truncated);
        QCOMPARE(viewFor(FileInfo::read(QUrl::fromLocalFile(path))), u"TextView"_s);
    }

    void cutInsideACharacter()
    {
        // One long line: the cut lands inside a two-byte character.
        QTemporaryDir dir;
        const QString path = dir.filePath(u"long.txt"_s);
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("a");
        for (int i = 0; i < 100; ++i) {
            f.write("æ");
        }
        f.close();
        bool truncated = false;
        QCOMPARE(readTextStart(path, 4, &truncated), u"aæ"_s); // "a", "æ", half an "æ"
        QVERIFY(truncated);
    }

    void animatedPictures()
    {
        QTemporaryDir dir;
        QFile gif(dir.filePath(u"moving.gif"_s));
        QVERIFY(gif.open(QIODevice::WriteOnly));
        gif.write(TwoFrameGif);
        gif.close();
        const FileInfo moving = FileInfo::read(QUrl::fromLocalFile(gif.fileName()));
        QCOMPARE(moving.imageSize, QSize(4, 4));
        QVERIFY(moving.animated);
        QCOMPARE(viewFor(moving), u"ImageView"_s);

        // One frame: still.
        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(dir.filePath(u"still.png"_s)));
        QVERIFY(!FileInfo::read(QUrl::fromLocalFile(dir.filePath(u"still.png"_s))).animated);
    }

    void mediaFiles()
    {
        FileInfo video;
        video.exists = true;
        video.mimeType = u"video/mp4"_s;
        QCOMPARE(viewFor(video), u"MediaView"_s);
        FileInfo audio;
        audio.exists = true;
        audio.mimeType = u"audio/mpeg"_s;
        QCOMPARE(viewFor(audio), u"MediaView"_s);
        FileInfo playlist; // audio/x-mpegurl is text: shown as text
        playlist.exists = true;
        playlist.mimeType = u"audio/x-mpegurl"_s;
        playlist.isText = true;
        QCOMPARE(viewFor(playlist), u"TextView"_s);
    }

    void fonts()
    {
        QVERIFY(isFont(u"font/ttf"_s));
        QVERIFY(isFont(u"font/woff2"_s));
        QVERIFY(isFont(u"application/x-font-type1"_s));
        QVERIFY(!isFont(u"application/x-font-afm"_s)); // metrics only, no glyphs
        QVERIFY(!isFont(u"text/plain"_s));
        // A real font file: read whole, shown by the font view.
        QDirIterator it(u"/usr/share/fonts"_s, {u"*.ttf"_s, u"*.otf"_s}, QDir::Files, QDirIterator::Subdirectories);
        if (!it.hasNext()) {
            QSKIP("no TrueType or OpenType font in /usr/share/fonts");
        }
        const QString path = it.next();
        const FileInfo font = FileInfo::read(QUrl::fromLocalFile(path));
        QVERIFY2(isFont(font.mimeType), qPrintable(font.mimeType));
        QCOMPARE(font.fontData.size(), QFileInfo(path).size());
        QCOMPARE(viewFor(font), u"FontView"_s);
        // Other files carry no font data.
        QTemporaryFile text;
        QVERIFY(text.open());
        text.write("text\n");
        text.close();
        QVERIFY(FileInfo::read(QUrl::fromLocalFile(text.fileName())).fontData.isEmpty());
    }

    void officeDocuments()
    {
        QVERIFY(isOfficeDocument(u"application/vnd.oasis.opendocument.text"_s));
        QVERIFY(isOfficeDocument(u"application/vnd.oasis.opendocument.spreadsheet"_s));
        QVERIFY(isOfficeDocument(u"application/vnd.openxmlformats-officedocument.wordprocessingml.document"_s));
        QVERIFY(isOfficeDocument(u"application/vnd.openxmlformats-officedocument.presentationml.presentation"_s));
        QVERIFY(isOfficeDocument(u"application/msword"_s));
        QVERIFY(isOfficeDocument(u"application/vnd.ms-excel"_s));
        QVERIFY(!isOfficeDocument(u"application/pdf"_s));
        QVERIFY(!isOfficeDocument(u"text/plain"_s));
        QVERIFY(!isOfficeDocument(u"application/zip"_s));
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
