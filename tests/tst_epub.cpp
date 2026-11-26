// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "epub.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

// EPUB books: kwiklook-archive --files on a real zip, then Epub::build.
class TstEpub : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    void write(const QString &path, const QByteArray &data)
    {
        const QString full = m_dir.filePath(u"book/"_s + path);
        QDir().mkpath(QFileInfo(full).absolutePath());
        QFile f(full);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data);
    }
    void picture(const QString &path, const QSize &size, Qt::GlobalColor colour)
    {
        const QString full = m_dir.filePath(u"book/"_s + path);
        QDir().mkpath(QFileInfo(full).absolutePath());
        QImage image(size, QImage::Format_RGB32);
        image.fill(colour);
        QVERIFY(image.save(full));
    }

    // The test book: a cover page, two chapters (one in a subfolder), an EPUB 3
    // table of contents with two levels, and things that must not get through.
    QString makeBook()
    {
        write(u"mimetype"_s, "application/epub+zip");
        write(u"META-INF/container.xml"_s,
              "<?xml version=\"1.0\"?><container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\"><rootfiles>"
              "<rootfile full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\"/></rootfiles></container>");
        write(u"OEBPS/content.opf"_s,
              "<?xml version=\"1.0\" encoding=\"UTF-8\"?><package xmlns=\"http://www.idpf.org/2007/opf\" version=\"3.0\">"
              "<metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\"><dc:title>The Example Book</dc:title><dc:creator>Ada Writer</dc:creator>"
              "<meta name=\"cover\" content=\"cover-img\"/></metadata><manifest>"
              "<item id=\"nav\" href=\"nav.xhtml\" media-type=\"application/xhtml+xml\" properties=\"nav\"/>"
              "<item id=\"cover-img\" href=\"images/cover.png\" media-type=\"image/png\"/>"
              "<item id=\"cover\" href=\"cover.xhtml\" media-type=\"application/xhtml+xml\"/>"
              "<item id=\"c1\" href=\"chapter%201.xhtml\" media-type=\"application/xhtml+xml\"/>"
              "<item id=\"c2\" href=\"text/chapter2.xhtml\" media-type=\"application/xhtml+xml\"/>"
              "<item id=\"broken\" href=\"broken.xhtml\" media-type=\"application/xhtml+xml\"/>"
              "</manifest><spine><itemref idref=\"cover\"/><itemref idref=\"c1\"/><itemref idref=\"c2\"/><itemref idref=\"broken\"/></spine></package>");
        write(u"OEBPS/nav.xhtml"_s,
              "<?xml version=\"1.0\" encoding=\"UTF-8\"?><html xmlns=\"http://www.w3.org/1999/xhtml\" xmlns:epub=\"http://www.idpf.org/2007/ops\"><body>"
              "<nav epub:type=\"landmarks\"><ol><li><a href=\"cover.xhtml\">Cover</a></li></ol></nav>"
              "<nav epub:type=\"toc\"><ol><li><a href=\"chapter%201.xhtml\">Chapter One</a><ol><li><a href=\"chapter%201.xhtml#part2\">A Second Part</a></li></ol></li>"
              "<li><a href=\"text/chapter2.xhtml\">Chapter Two</a></li></ol></nav></body></html>");
        write(u"OEBPS/cover.xhtml"_s,
              "<?xml version=\"1.0\"?><html xmlns=\"http://www.w3.org/1999/xhtml\"><body><div><svg xmlns=\"http://www.w3.org/2000/svg\" "
              "xmlns:xlink=\"http://www.w3.org/1999/xlink\"><image xlink:href=\"images/cover.png\"/></svg></div></body></html>");
        write(u"OEBPS/chapter 1.xhtml"_s,
              "<?xml version=\"1.0\" encoding=\"UTF-8\"?><html xmlns=\"http://www.w3.org/1999/xhtml\"><head><title>One</title><style>p{color:red}</style>"
              "<script>alert(1)</script></head><body><h1>Chapter One</h1><p>It was a <em>bright</em>&nbsp;cold day.</p>"
              "<p><img src=\"images/pic.png\" alt=\"A picture\"/></p><script>document.write(\"bad\")</script>"
              "<h2 id=\"part2\">A Second Part</h2><p>See <a href=\"https://example.org/\">the site</a> and <a href=\"text/chapter2.xhtml\">chapter two</a>.</p>"
              "<iframe src=\"https://example.org/tracker\"></iframe><img src=\"https://example.org/remote.png\"/><form><input value=\"x\"/></form></body></html>");
        write(u"OEBPS/text/chapter2.xhtml"_s,
              "<?xml version=\"1.0\" encoding=\"UTF-8\"?><html xmlns=\"http://www.w3.org/1999/xhtml\"><body><h1>Chapter Two</h1>"
              "<p>The end: <img src=\"../images/pic.png\"/></p><table><tr><td colspan=\"2\" onclick=\"run\">cell</td></tr></table></body></html>");
        write(u"OEBPS/broken.xhtml"_s, "<html xmlns=\"http://www.w3.org/1999/xhtml\"><body><p>Readable start<p>then broken</body>");
        picture(u"OEBPS/images/cover.png"_s, QSize(400, 600), Qt::darkBlue);
        picture(u"OEBPS/images/pic.png"_s, QSize(1200, 300), Qt::darkYellow);

        const QString bsdtar = QStandardPaths::findExecutable(u"bsdtar"_s);
        if (bsdtar.isEmpty()) {
            return {};
        }
        const QString book = m_dir.filePath(u"example.epub"_s);
        QProcess p;
        p.setWorkingDirectory(m_dir.filePath(u"book"_s));
        p.start(bsdtar, {u"--format"_s, u"zip"_s, u"-cf"_s, book, u"mimetype"_s, u"META-INF"_s, u"OEBPS"_s});
        p.waitForFinished();
        return p.exitCode() == 0 ? book : QString();
    }

private Q_SLOTS:
    void readsABook()
    {
        const QString file = makeBook();
        if (file.isEmpty()) {
            QSKIP("bsdtar is needed to make the test book");
        }
        QProcess helper;
        helper.start(QStringLiteral(ARCHIVE_HELPER), QStringList{u"--files"_s, file, u"8000000"_s, u"64000000"_s} + Epub::wantedSuffixes());
        QVERIFY(helper.waitForFinished());
        const Epub::Files files = Epub::parseHelperOutput(helper.readAllStandardOutput());
        QCOMPARE(files.status, u"ok"_s);
        QVERIFY(files.files.contains(u"OEBPS/chapter 1.xhtml"_s));
        QVERIFY(!files.files.contains(u"mimetype"_s)); // not asked for

        const Epub::Book book = Epub::build(files);
        QVERIFY(book.ok);
        QCOMPARE(book.title, u"The Example Book"_s);
        QCOMPARE(book.author, u"Ada Writer"_s);
        QVERIFY(!book.truncated);
        const QString &html = book.html;
        // The text, in reading order, with its formatting.
        QVERIFY(html.contains(u"<em>bright</em>"_s));
        QVERIFY(html.indexOf(u"Chapter One"_s) < html.indexOf(u"Chapter Two"_s));
        QVERIFY(html.contains(QChar(0xa0) + u"cold day"_s)); // &nbsp; without a DTD
        QVERIFY(html.contains(u"<td colspan=\"2\">cell</td>"_s));
        QVERIFY(html.contains(u"Readable start"_s)); // a broken chapter: what could be read
        // Nothing that runs, styles or fetches; links only to the web.
        for (const QString &absent : {u"alert"_s, u"document.write"_s, u"color:red"_s, u"iframe"_s, u"tracker"_s, u"remote.png"_s, u"onclick"_s,
                                      u"<form"_s, u"<input"_s, u"href=\"text/chapter2"_s}) {
            QVERIFY2(!html.contains(absent), qPrintable(absent));
        }
        QVERIFY(html.contains(u"<a href=\"https://example.org/\">the site</a>"_s));
        // Pictures inside the book as data, scaled to the column; the cover page is
        // left out (the cover shows above the title), so the cover appears once.
        QCOMPARE(html.count(u"<img src=\"data:image/png;base64,"_s), 3); // cover, pic in both chapters
        QVERIFY(html.contains(u"width=\"600\" height=\"150\""_s)); // 1200 × 300 halved
        QVERIFY(html.indexOf(u"<img"_s) < html.indexOf(u"<h1 align=\"center\">The Example Book</h1>"_s));
        // The book's table of contents, with anchors that are in the text.
        QCOMPARE(book.contents.size(), 3);
        const QVariantMap second = book.contents.at(1).toMap();
        QCOMPARE(second.value(u"title"_s).toString(), u"A Second Part"_s);
        QCOMPARE(second.value(u"level"_s).toInt(), 2);
        QCOMPARE(second.value(u"anchor"_s).toString(), u"OEBPS/chapter 1.xhtml#part2"_s);
        for (const QVariant &entry : book.contents) {
            QVERIFY(html.contains(u"<a name=\"%1\"></a>"_s.arg(entry.toMap().value(u"anchor"_s).toString().toHtmlEscaped())));
        }
    }

    void longBooksShowTheirStart()
    {
        Epub::Files files;
        files.status = u"ok"_s;
        files.files.insert(u"META-INF/container.xml"_s, "<container><rootfiles><rootfile full-path=\"p.opf\"/></rootfiles></container>");
        QByteArray spine;
        QByteArray manifest;
        const QByteArray paragraph = "<p>" + QByteArray(1000, 'x') + "</p>";
        for (int i = 0; i < 3; ++i) {
            const QByteArray name = "c" + QByteArray::number(i);
            manifest += "<item id=\"" + name + "\" href=\"" + name + ".xhtml\"/>";
            spine += "<itemref idref=\"" + name + "\"/>";
            files.files.insert(QString::fromLatin1(name + ".xhtml"), "<html><body>" + paragraph.repeated(600) + "</body></html>");
        }
        files.files.insert(u"p.opf"_s, "<package><manifest>" + manifest + "</manifest><spine>" + spine + "</spine></package>");
        const Epub::Book book = Epub::build(files);
        QVERIFY(book.ok);
        QVERIFY(book.truncated);
        QVERIFY(book.html.size() < 2 * Epub::TextLimit);
        // The helper stopping early also counts as only the start.
        files.status = u"limit"_s;
        files.files.remove(u"c1.xhtml"_s);
        files.files.remove(u"c2.xhtml"_s);
        QVERIFY(Epub::build(files).truncated);
    }

    void readsTheHelpersOutput()
    {
        const QByteArray output = "f\t3\ta\\tb.xhtml\nabc\ns\t9000\tbig.png\nf\t0\tempty.xml\n\nend\t2\t3\tok\t\n";
        const Epub::Files files = Epub::parseHelperOutput(output);
        QCOMPARE(files.files.value(u"a\tb.xhtml"_s), QByteArray("abc"));
        QVERIFY(files.files.contains(u"empty.xml"_s));
        QVERIFY(!files.files.contains(u"big.png"_s));
        QCOMPARE(files.status, u"ok"_s);
        // Cut short: what came before stays, no end line means no status.
        const Epub::Files cut = Epub::parseHelperOutput("f\t3\tx.xml\nabc\nf\t100\ty.xml\nshort");
        QVERIFY(cut.files.contains(u"x.xml"_s));
        QVERIFY(!cut.files.contains(u"y.xml"_s));
        QVERIFY(cut.status.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstEpub)
#include "tst_epub.moc"
