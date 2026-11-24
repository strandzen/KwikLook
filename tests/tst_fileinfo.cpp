// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
// File details and the choice of view, on scratch files.
#include "cards.h"
#include "fileinfo.h"

#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QBuffer>
#include <QTimeZone>
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

    void formattedKinds()
    {
        QTemporaryDir dir;
        const auto write = [&dir](const QString &name, const QByteArray &data) {
            QFile f(dir.filePath(name));
            if (f.open(QIODevice::WriteOnly)) {
                f.write(data);
            }
            f.close();
            return FileInfo::read(QUrl::fromLocalFile(f.fileName()));
        };
        QCOMPARE(write(u"a.md"_s, "# Hi\n").formattedKind, u"markdown"_s);
        QCOMPARE(write(u"a.html"_s, "<html><body>Hi</body></html>\n").formattedKind, u"html"_s);
        QCOMPARE(write(u"a.txt"_s, "plain\n").formattedKind, QString());
        // JSON: indented when it parses; the source otherwise.
        const FileInfo json = write(u"a.json"_s, "{\"a\":[1,2],\"b\":{\"c\":true}}");
        QCOMPARE(json.formattedKind, u"json"_s);
        QVERIFY(json.formattedText.contains(u"\n    \"a\": [\n"_s));
        QCOMPARE(viewFor(json), u"TextView"_s);
        QVERIFY(write(u"b.json"_s, "{broken").formattedText.isEmpty());
        // SVG: the picture formatted, its text as source.
        const FileInfo svg = write(u"a.svg"_s, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"4\" height=\"4\"><rect width=\"4\" height=\"4\"/></svg>");
        QCOMPARE(svg.formattedKind, u"svg"_s);
        QCOMPARE(viewFor(svg, true), u"ImageView"_s);
        QCOMPARE(viewFor(svg, false), u"TextView"_s);
        // .desktop: the card, read by KDesktopFile.
        const FileInfo desktop = write(u"a.desktop"_s, "[Desktop Entry]\nType=Application\nName=Example\nComment=Does things\nIcon=kate\nExec=kate %U\nCategories=Utility;TextEditor;\nTerminal=true\n");
        QCOMPARE(desktop.formattedKind, u"desktop"_s);
        QCOMPARE(viewFor(desktop, true), u"DesktopView"_s);
        QCOMPARE(viewFor(desktop, false), u"TextView"_s);
        QCOMPARE(desktop.desktopEntry.value(u"name"_s).toString(), u"Example"_s);
        QCOMPARE(desktop.desktopEntry.value(u"exec"_s).toString(), u"kate %U"_s);
        QCOMPARE(desktop.desktopEntry.value(u"categories"_s).toString(), u"Utility, TextEditor"_s);
        QCOMPARE(desktop.desktopEntry.value(u"terminal"_s).toBool(), true);
    }

    void delimitedText()
    {
        using Rows = QList<QStringList>;
        QCOMPARE(parseDelimited(u"a,b\n1,2\n"_s), Rows({{u"a"_s, u"b"_s}, {u"1"_s, u"2"_s}}));
        // Quotes: separators, line breaks and doubled quotes inside; Windows line ends.
        QCOMPARE(parseDelimited(u"name,note\r\n\"Doe, J\",\"said \"\"hi\"\"\nthen left\"\r\n"_s),
                 Rows({{u"name"_s, u"note"_s}, {u"Doe, J"_s, u"said \"hi\"\nthen left"_s}}));
        // Empty fields count; blank lines don't.
        QCOMPARE(parseDelimited(u"a,,c\n\n,b,\n"_s), Rows({{u"a"_s, QString(), u"c"_s}, {QString(), u"b"_s, QString()}}));
        // The separator: the most frequent of , ; tab in the first line (European CSV).
        QCOMPARE(parseDelimited(u"a;b;c\n1,5;2;3\n"_s), Rows({{u"a"_s, u"b"_s, u"c"_s}, {u"1,5"_s, u"2"_s, u"3"_s}}));
        QCOMPARE(parseDelimited(u"a\tb\n1\t2"_s), Rows({{u"a"_s, u"b"_s}, {u"1"_s, u"2"_s}}));
        // A real file: kind, rows, view.
        QTemporaryDir dir;
        QFile f(dir.filePath(u"t.csv"_s));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("x,y\n1,2\n3,4\n");
        f.close();
        const FileInfo csv = FileInfo::read(QUrl::fromLocalFile(f.fileName()));
        QCOMPARE(csv.formattedKind, u"csv"_s);
        QCOMPARE(csv.tableRows.size(), 3);
        QCOMPARE(viewFor(csv, true), u"CsvView"_s);
        QCOMPARE(viewFor(csv, false), u"TextView"_s);
    }

    void contactAndEventCards()
    {
        QTemporaryDir dir;
        const auto read = [&dir](const QString &name, const QByteArray &data) {
            QFile f(dir.filePath(name));
            if (f.open(QIODevice::WriteOnly)) {
                f.write(data);
            }
            f.close();
            return FileInfo::read(QUrl::fromLocalFile(f.fileName()));
        };
        QImage photo(8, 8, QImage::Format_RGB32);
        photo.fill(Qt::blue);
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        photo.save(&buffer, "PNG");
        const FileInfo people = read(u"people.vcf"_s,
                                     "BEGIN:VCARD\nVERSION:3.0\nFN:Ada Lovelace\nN:Lovelace;Ada;;;\nORG:Engines\nTEL;TYPE=CELL:+44 1\n"
                                     "EMAIL:ada@example.org\nBDAY:1815-12-10\nPHOTO;ENCODING=b;TYPE=PNG:" + png.toBase64() + "\nEND:VCARD\n"
                                     "BEGIN:VCARD\nVERSION:3.0\nFN:Grace Hopper\nN:Hopper;Grace;;;\nEND:VCARD\n");
        QCOMPARE(people.formattedKind, u"vcard"_s);
        QCOMPARE(viewFor(people), u"CardsView"_s);
        QCOMPARE(people.cards.size(), 2);
        const QVariantMap ada = people.cards.first().toMap();
        QCOMPARE(ada.value(u"kind"_s).toString(), u"contact"_s);
        QCOMPARE(ada.value(u"name"_s).toString(), u"Ada Lovelace"_s);
        QCOMPARE(ada.value(u"organization"_s).toString(), u"Engines"_s);
        QCOMPARE(ada.value(u"emails"_s).toStringList(), QStringList{u"ada@example.org"_s});
        QCOMPARE(ada.value(u"phones"_s).toList().first().toMap().value(u"value"_s).toString(), u"+44 1"_s);
        QCOMPARE(ada.value(u"birthday"_s).toDate(), QDate(1815, 12, 10));
        QVERIFY(ada.value(u"photo"_s).toString().startsWith(u"data:image/png;base64,"_s));
        QVERIFY(people.cards.last().toMap().value(u"photo"_s).toString().isEmpty());

        const FileInfo events = read(u"events.ics"_s,
                                     "BEGIN:VCALENDAR\nVERSION:2.0\nPRODID:-//test//EN\n"
                                     "BEGIN:VEVENT\nUID:1\nDTSTAMP:20261120T090000Z\nDTSTART;VALUE=DATE:20261225\nDTEND;VALUE=DATE:20261226\nSUMMARY:Christmas\nEND:VEVENT\n"
                                     "BEGIN:VEVENT\nUID:2\nDTSTAMP:20261120T090000Z\nDTSTART:20261124T090000Z\nDTEND:20261124T100000Z\nSUMMARY:Meeting\n"
                                     "LOCATION:Room 4\nRRULE:FREQ=WEEKLY\nEND:VEVENT\nEND:VCALENDAR\n");
        QCOMPARE(events.formattedKind, u"calendar"_s);
        QCOMPARE(events.cards.size(), 2);
        const QVariantMap meeting = events.cards.first().toMap(); // by start date
        QCOMPARE(meeting.value(u"summary"_s).toString(), u"Meeting"_s);
        QCOMPARE(meeting.value(u"location"_s).toString(), u"Room 4"_s);
        QCOMPARE(meeting.value(u"repeats"_s).toBool(), true);
        QCOMPARE(meeting.value(u"start"_s).toDateTime().toUTC(), QDateTime(QDate(2026, 11, 24), QTime(9, 0), QTimeZone::UTC));
        QCOMPARE(events.cards.last().toMap().value(u"allDay"_s).toBool(), true);
        // Not a calendar after all: no cards, no crash.
        QCOMPARE(read(u"broken.ics"_s, "BEGIN:VCALENDAR\ngarbage\n").cards.size(), 0);

        // Without the card library (KContacts, KCalendarCore not installed): plain text,
        // no view toggle (no formatted kind).
        Cards::setLibraryPath(u"/nonexistent/kwiklook-cards.so"_s);
        const FileInfo plain = read(u"plain.vcf"_s, "BEGIN:VCARD\nVERSION:3.0\nFN:Ada Lovelace\nEND:VCARD\n");
        Cards::setLibraryPath(QString()); // back to the real one
        QVERIFY(plain.formattedKind.isEmpty());
        QVERIFY(plain.cards.isEmpty());
        QCOMPARE(viewFor(plain), u"TextView"_s);
        QVERIFY(plain.text.contains(u"Ada Lovelace"_s));
        QVERIFY(Cards::available());
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
