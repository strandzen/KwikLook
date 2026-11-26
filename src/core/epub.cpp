// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "epub.h"

#include <QBuffer>
#include <QDir>
#include <QImageReader>
#include <QSet>
#include <QUrl>
#include <QXmlStreamEntityResolver>
#include <QXmlStreamReader>

#include <utility>

using namespace Qt::StringLiterals;

namespace Epub
{
namespace
{
// "a\tb" etc. as kwiklook-archive escapes names.
QString unescaped(const QByteArray &text)
{
    QByteArray out;
    out.reserve(text.size());
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text.at(i) == '\\' && i + 1 < text.size()) {
            const char c = text.at(++i);
            out += c == 't' ? '\t' : c == 'n' ? '\n' : c == 'r' ? '\r' : c;
        } else {
            out += text.at(i);
        }
    }
    return QString::fromUtf8(out);
}

// The folder part of a path in the book, with its slash ("OEBPS/text/").
QString folderOf(const QString &path)
{
    const qsizetype slash = path.lastIndexOf(u'/');
    return slash < 0 ? QString() : path.left(slash + 1);
}

// A reference from a file in `folder` (href="../images/a.png#x") as a path in the
// book, without the fragment; empty for addresses outside the book.
QString resolve(const QString &folder, const QString &href, QString *fragment = nullptr)
{
    QString target = href;
    const qsizetype hash = target.indexOf(u'#');
    if (fragment) {
        *fragment = hash < 0 ? QString() : target.mid(hash + 1);
    }
    if (hash >= 0) {
        target.truncate(hash);
    }
    if (target.contains(u':')) {
        return {}; // http:, data:, ...
    }
    target = QUrl::fromPercentEncoding(target.toUtf8());
    if (target.isEmpty()) {
        return {};
    }
    const QString path = QDir::cleanPath(folder + target);
    return path.startsWith(u"../"_s) || path.startsWith(u'/') ? QString() : path;
}

// HTML's named characters, which XHTML chapters use without declaring them
// (QXmlStreamReader stops at an undeclared entity otherwise).
class HtmlEntities : public QXmlStreamEntityResolver
{
public:
    QString resolveUndeclaredEntity(const QString &name) override
    {
        static const QHash<QString, QString> names{
            {u"nbsp"_s, QString(QChar(0xa0))},   {u"shy"_s, QString(QChar(0xad))},    {u"mdash"_s, QString(QChar(0x2014))},
            {u"ndash"_s, QString(QChar(0x2013))}, {u"hellip"_s, QString(QChar(0x2026))}, {u"lsquo"_s, QString(QChar(0x2018))},
            {u"rsquo"_s, QString(QChar(0x2019))}, {u"ldquo"_s, QString(QChar(0x201c))}, {u"rdquo"_s, QString(QChar(0x201d))},
            {u"laquo"_s, QString(QChar(0xab))},   {u"raquo"_s, QString(QChar(0xbb))},   {u"copy"_s, QString(QChar(0xa9))},
            {u"reg"_s, QString(QChar(0xae))},     {u"trade"_s, QString(QChar(0x2122))}, {u"deg"_s, QString(QChar(0xb0))},
            {u"middot"_s, QString(QChar(0xb7))},  {u"bull"_s, QString(QChar(0x2022))},  {u"times"_s, QString(QChar(0xd7))},
            {u"eacute"_s, QString(QChar(0xe9))},  {u"egrave"_s, QString(QChar(0xe8))},  {u"aacute"_s, QString(QChar(0xe1))},
            {u"agrave"_s, QString(QChar(0xe0))},  {u"ouml"_s, QString(QChar(0xf6))},    {u"uuml"_s, QString(QChar(0xfc))},
            {u"auml"_s, QString(QChar(0xe4))},    {u"szlig"_s, QString(QChar(0xdf))},   {u"ccedil"_s, QString(QChar(0xe7))},
            {u"thinsp"_s, QString(QChar(0x2009))}, {u"ensp"_s, QString(QChar(0x2002))},  {u"emsp"_s, QString(QChar(0x2003))},
        };
        return names.value(name);
    }
};

// Elements kept as they are, and how others are written.
const QSet<QString> &keptElements()
{
    static const QSet<QString> kept{
        u"p"_s,  u"div"_s, u"h1"_s,    u"h2"_s,    u"h3"_s,     u"h4"_s,    u"h5"_s,      u"h6"_s,    u"em"_s,  u"i"_s,     u"strong"_s, u"b"_s,
        u"u"_s,  u"s"_s,   u"del"_s,   u"sub"_s,   u"sup"_s,    u"small"_s, u"big"_s,     u"cite"_s,  u"q"_s,   u"code"_s,  u"tt"_s,     u"kbd"_s,
        u"var"_s, u"samp"_s, u"pre"_s, u"blockquote"_s, u"ul"_s, u"ol"_s,   u"li"_s,      u"dl"_s,    u"dt"_s,  u"dd"_s,    u"table"_s,  u"thead"_s,
        u"tbody"_s, u"tfoot"_s, u"tr"_s, u"th"_s,  u"td"_s,     u"caption"_s,
    };
    return kept;
}
QString writtenAs(const QString &name)
{
    if (keptElements().contains(name)) {
        return name;
    }
    static const QHash<QString, QString> aliases{
        {u"section"_s, u"div"_s}, {u"article"_s, u"div"_s}, {u"aside"_s, u"div"_s},    {u"header"_s, u"div"_s}, {u"footer"_s, u"div"_s},
        {u"main"_s, u"div"_s},    {u"figure"_s, u"div"_s},  {u"figcaption"_s, u"p"_s}, {u"ins"_s, u"u"_s},      {u"mark"_s, u"b"_s},
    };
    return aliases.value(name); // empty: the tag goes, its text stays
}
// Left out with everything inside.
bool droppedWhole(const QString &name)
{
    static const QSet<QString> dropped{
        u"script"_s, u"style"_s, u"iframe"_s, u"object"_s, u"embed"_s,  u"form"_s,  u"input"_s, u"button"_s, u"select"_s,
        u"textarea"_s, u"audio"_s, u"video"_s, u"canvas"_s, u"template"_s, u"noscript"_s, u"head"_s, u"title"_s,  u"math"_s,
    };
    return dropped.contains(name);
}

class Builder
{
public:
    explicit Builder(const Files &files)
        : m_files(files.files)
    {
    }

    Book build(bool helperStoppedEarly)
    {
        Book book;
        const QString packagePath = rootFile();
        const QByteArray package = m_files.value(packagePath);
        if (package.isEmpty()) {
            return book;
        }
        readPackage(package, folderOf(packagePath));
        book.title = m_title;
        book.author = m_author;

        QString body;
        for (const QString &id : std::as_const(m_spine)) {
            const QString path = m_hrefs.value(id);
            if (!m_files.contains(path) || m_textBytes > TextLimit) {
                continue;
            }
            const QString chapter = sanitizedChapter(path);
            if (!chapter.isEmpty()) {
                body += chapter;
                m_chapters << path;
            }
        }
        if (m_chapters.isEmpty()) {
            return book;
        }
        book.ok = true;
        book.truncated = m_textBytes > TextLimit || helperStoppedEarly;

        QString head;
        if (const QString cover = picture(m_hrefs.value(m_coverId)); !cover.isEmpty()) {
            head += u"<p align=\"center\">"_s + cover + u"</p>"_s;
        }
        if (!m_title.isEmpty()) {
            head += u"<h1 align=\"center\">"_s + m_title.toHtmlEscaped() + u"</h1>"_s;
        }
        if (!m_author.isEmpty()) {
            head += u"<p align=\"center\">"_s + m_author.toHtmlEscaped() + u"</p>"_s;
        }
        if (!head.isEmpty()) {
            head += u"<hr/>"_s;
        }
        book.html = head + body;
        book.contents = tableOfContents();
        return book;
    }

private:
    QString rootFile() const
    {
        QXmlStreamReader xml(m_files.value(u"META-INF/container.xml"_s));
        while (!xml.atEnd()) {
            if (xml.readNext() == QXmlStreamReader::StartElement && xml.name() == u"rootfile") {
                return xml.attributes().value(u"full-path").toString();
            }
        }
        return {};
    }

    void readPackage(const QByteArray &data, const QString &folder)
    {
        QXmlStreamReader xml(data);
        QString coverMeta;
        QString ncxId;
        while (!xml.atEnd()) {
            if (xml.readNext() != QXmlStreamReader::StartElement) {
                continue;
            }
            const auto name = xml.name();
            const QXmlStreamAttributes a = xml.attributes();
            if (name == u"title" && m_title.isEmpty()) {
                m_title = xml.readElementText().simplified();
            } else if (name == u"creator" && m_author.isEmpty()) {
                m_author = xml.readElementText().simplified();
            } else if (name == u"meta" && a.value(u"name") == u"cover") {
                coverMeta = a.value(u"content").toString();
            } else if (name == u"item") {
                const QString id = a.value(u"id").toString();
                m_hrefs.insert(id, resolve(folder, a.value(u"href").toString()));
                m_types.insert(m_hrefs.value(id), a.value(u"media-type").toString());
                const QStringList properties = a.value(u"properties").toString().split(u' ', Qt::SkipEmptyParts);
                if (properties.contains(u"cover-image"_s)) {
                    m_coverId = id;
                }
                if (properties.contains(u"nav"_s)) {
                    m_navId = id;
                }
            } else if (name == u"spine") {
                ncxId = a.value(u"toc").toString();
            } else if (name == u"itemref" && a.value(u"linear") != u"no") {
                m_spine << a.value(u"idref").toString();
            }
        }
        if (m_coverId.isEmpty()) {
            m_coverId = coverMeta;
        }
        if (m_navId.isEmpty()) {
            m_ncxId = ncxId;
        }
    }

    // A picture in the book as an <img>, at most PictureWidth wide; empty if it's
    // missing, unreadable or over the budget.
    QString picture(const QString &path, const QString &alt = QString())
    {
        if (path.isEmpty()) {
            return {};
        }
        if (const auto known = m_pictures.constFind(path); known != m_pictures.cend()) {
            return *known;
        }
        QString tag;
        const QByteArray data = m_files.value(path);
        if (!data.isEmpty() && m_pictureBytes + data.size() <= PictureLimit) {
            QBuffer buffer;
            buffer.setData(data);
            QImageReader reader(&buffer);
            const QSize size = reader.size();
            const QByteArray format = reader.format();
            if (size.isValid() && !format.isEmpty()) {
                m_pictureBytes += data.size();
                const QString type = format == "svg" ? u"image/svg+xml"_s : u"image/"_s + QString::fromLatin1(format);
                const int width = qMin(size.width(), PictureWidth);
                const int height = qMax(1, size.height() * width / size.width());
                tag = u"<img src=\"data:%1;base64,%2\" width=\"%3\" height=\"%4\" alt=\"%5\"/>"_s.arg(type, QString::fromLatin1(data.toBase64()))
                          .arg(width)
                          .arg(height)
                          .arg(alt.toHtmlEscaped());
            }
        }
        m_pictures.insert(path, tag);
        return tag;
    }

    QString sanitizedChapter(const QString &path)
    {
        const QString folder = folderOf(path);
        HtmlEntities entities;
        QXmlStreamReader xml(m_files.value(path));
        xml.setEntityResolver(&entities);
        QString out;
        // Anchors for the table of contents wait for the next text or picture: Qt
        // keeps an empty <a name> only inside a block, not between two.
        QString anchors = u"<a name=\"%1\"></a>"_s.arg(path.toHtmlEscaped());
        QStringList open; // closing tag per open element ("" = none written)
        bool inBody = false;
        int skipping = 0;
        QString text; // the chapter's own text, to spot a page that only holds the cover
        int pictures = 0;
        bool onlyCover = true;
        while (!xml.atEnd() && !xml.hasError()) {
            const auto token = xml.readNext();
            if (token == QXmlStreamReader::StartElement) {
                const QString name = xml.name().toString().toLower();
                if (!inBody) {
                    inBody = name == u"body";
                    continue;
                }
                if (skipping > 0 || droppedWhole(name)) {
                    ++skipping;
                    continue;
                }
                const QXmlStreamAttributes a = xml.attributes();
                if (const QString id = a.value(u"id").toString(); !id.isEmpty()) {
                    anchors += u"<a name=\"%1#%2\"></a>"_s.arg(path.toHtmlEscaped(), id.toHtmlEscaped());
                }
                if (name == u"img" || name == u"image") { // <image> in SVG covers
                    QString href = a.value(u"src").toString();
                    if (href.isEmpty()) {
                        href = a.value(u"href").toString();
                    }
                    if (href.isEmpty()) {
                        href = a.value(u"http://www.w3.org/1999/xlink", u"href").toString();
                    }
                    const QString target = resolve(folder, href);
                    if (const QString tag = picture(target, a.value(u"alt").toString()); !tag.isEmpty()) {
                        out += std::exchange(anchors, QString()) + tag;
                    }
                    ++pictures;
                    onlyCover = onlyCover && target == m_hrefs.value(m_coverId);
                    open << QString();
                    continue;
                }
                if (name == u"br" || name == u"hr") {
                    out += u'<' + name + u"/>"_s;
                    open << QString();
                    continue;
                }
                if (name == u"a") {
                    const QString href = a.value(u"href").toString();
                    if (href.startsWith(u"http://"_s) || href.startsWith(u"https://"_s) || href.startsWith(u"mailto:"_s)) {
                        out += u"<a href=\"%1\">"_s.arg(href.toHtmlEscaped());
                        open << u"a"_s;
                    } else {
                        open << QString(); // inside the book: no link, only the text
                    }
                    continue;
                }
                const QString tag = writtenAs(name);
                if (tag.isEmpty()) {
                    open << QString();
                    continue;
                }
                QString attributes;
                for (const auto kept : {u"colspan", u"rowspan", u"start"}) {
                    if (const auto value = a.value(kept); !value.isEmpty()) {
                        attributes += u" %1=\"%2\""_s.arg(kept, value.toString().toHtmlEscaped());
                    }
                }
                out += u'<' + tag + attributes + u'>';
                open << tag;
            } else if (token == QXmlStreamReader::EndElement) {
                if (!inBody) {
                    continue;
                }
                if (skipping > 0) {
                    --skipping;
                    continue;
                }
                if (open.isEmpty()) {
                    inBody = false; // </body>
                    continue;
                }
                if (const QString tag = open.takeLast(); !tag.isEmpty()) {
                    out += u"</"_s + tag + u'>';
                }
            } else if (token == QXmlStreamReader::Characters && inBody && skipping == 0) {
                const QString chunk = xml.text().toString();
                text += chunk;
                m_textBytes += chunk.size();
                if (!chunk.trimmed().isEmpty()) {
                    out += std::exchange(anchors, QString());
                }
                out += chunk.toHtmlEscaped();
            }
        }
        while (!open.isEmpty()) { // a chapter that isn't well-formed: what was read, closed
            if (const QString tag = open.takeLast(); !tag.isEmpty()) {
                out += u"</"_s + tag + u'>';
            }
        }
        // A page holding only the cover: the cover is shown above the title already.
        if (text.trimmed().isEmpty() && (pictures == 0 || onlyCover)) {
            return {};
        }
        return out;
    }

    QVariantList tableOfContents() const
    {
        QVariantList entries;
        const auto add = [&](const QString &title, int level, const QString &folder, const QString &href) {
            QString fragment;
            const QString path = resolve(folder, href, &fragment);
            if (title.isEmpty() || !m_chapters.contains(path)) {
                return;
            }
            entries << QVariantMap{{u"title"_s, title}, {u"level"_s, level}, {u"anchor"_s, fragment.isEmpty() ? path : path + u'#' + fragment}};
        };
        if (const QString nav = m_hrefs.value(m_navId); !nav.isEmpty()) {
            // EPUB 3: <nav epub:type="toc"><ol><li><a href>…</a><ol>…
            HtmlEntities entities;
            QXmlStreamReader xml(m_files.value(nav));
            xml.setEntityResolver(&entities);
            int navDepth = 0;
            int lists = 0;
            while (!xml.atEnd() && !xml.hasError()) {
                const auto token = xml.readNext();
                if (token == QXmlStreamReader::StartElement) {
                    const auto name = xml.name();
                    if (name == u"nav") {
                        if (navDepth > 0 || xml.attributes().value(u"http://www.idpf.org/2007/ops", u"type").contains(u"toc")) {
                            ++navDepth;
                        }
                    } else if (navDepth > 0 && name == u"ol") {
                        ++lists;
                    } else if (navDepth > 0 && (name == u"a" || name == u"span")) {
                        const QString href = xml.attributes().value(u"href").toString();
                        add(xml.readElementText(QXmlStreamReader::IncludeChildElements).simplified(), lists, folderOf(nav), href);
                    }
                } else if (token == QXmlStreamReader::EndElement) {
                    if (xml.name() == u"nav" && navDepth > 0) {
                        --navDepth;
                    } else if (navDepth > 0 && xml.name() == u"ol") {
                        --lists;
                    }
                }
            }
        } else if (const QString ncx = m_hrefs.value(m_ncxId); !ncx.isEmpty()) {
            // EPUB 2: <navPoint><navLabel><text>…</text></navLabel><content src/><navPoint>…
            QXmlStreamReader xml(m_files.value(ncx));
            int depth = 0;
            QString label;
            while (!xml.atEnd() && !xml.hasError()) {
                const auto token = xml.readNext();
                if (token == QXmlStreamReader::StartElement) {
                    if (xml.name() == u"navPoint") {
                        ++depth;
                        label.clear();
                    } else if (xml.name() == u"text" && depth > 0) {
                        label = xml.readElementText().simplified();
                    } else if (xml.name() == u"content" && depth > 0) {
                        add(label, depth, folderOf(ncx), xml.attributes().value(u"src").toString());
                    }
                } else if (token == QXmlStreamReader::EndElement && xml.name() == u"navPoint") {
                    --depth;
                }
            }
        }
        return entries;
    }

    const QHash<QString, QByteArray> &m_files;
    QString m_title;
    QString m_author;
    QHash<QString, QString> m_hrefs; // manifest id → path in the book
    QHash<QString, QString> m_types;
    QStringList m_spine;
    QString m_coverId;
    QString m_navId;
    QString m_ncxId;
    QStringList m_chapters; // shown, in order
    QHash<QString, QString> m_pictures; // path → <img> (or empty)
    qsizetype m_pictureBytes = 0;
    qsizetype m_textBytes = 0;
};
}

Files parseHelperOutput(const QByteArray &output)
{
    Files result;
    qsizetype at = 0;
    while (at < output.size()) {
        const qsizetype end = output.indexOf('\n', at);
        if (end < 0) {
            break;
        }
        const QList<QByteArray> fields = output.mid(at, end - at).split('\t');
        at = end + 1;
        if (fields.value(0) == "f" && fields.size() >= 3) {
            bool ok = false;
            const qsizetype size = fields.at(1).toLongLong(&ok);
            if (!ok || size < 0 || at + size > output.size()) {
                break; // cut short
            }
            result.files.insert(unescaped(fields.at(2)), output.mid(at, size));
            at += size + 1; // the bytes and their line break
        } else if (fields.value(0) == "end" && fields.size() >= 4) {
            result.status = QString::fromUtf8(fields.at(3));
            result.message = QString::fromUtf8(fields.value(4));
        }
    }
    return result;
}

QStringList wantedSuffixes()
{
    return {u".xml"_s, u".opf"_s, u".ncx"_s, u".xhtml"_s, u".html"_s, u".htm"_s,  u".jpg"_s,
            u".jpeg"_s, u".png"_s, u".gif"_s, u".svg"_s,  u".webp"_s,  u".avif"_s};
}

Book build(const Files &files)
{
    return Builder(files).build(files.status == u"limit"_s || files.status == u"time"_s);
}
}
