// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "fileinfo.h"

#include "archivelisting.h"
#include "cards.h"

#include <KConfigGroup>
#include <KDesktopFile>
#include <KFileMetaData/ExtractorCollection>
#include <KFileMetaData/PropertyInfo>
#include <KFileMetaData/SimpleExtractionResult>

#include <QFileInfo>
#include <QJsonDocument>
#include <QStringDecoder>
#include <QImageReader>

#include <algorithm>
#include <QMimeDatabase>

using namespace Qt::StringLiterals;

namespace
{
QString formattedKindOf(const QMimeType &mime)
{
    if (mime.inherits(u"text/markdown"_s)) {
        return u"markdown"_s;
    }
    if (mime.inherits(u"text/html"_s)) {
        return u"html"_s;
    }
    if (mime.name() == u"image/svg+xml"_s) {
        return u"svg"_s;
    }
    if (mime.inherits(u"application/json"_s)) {
        return u"json"_s;
    }
    if (mime.inherits(u"application/x-desktop"_s)) {
        return u"desktop"_s;
    }
    if (mime.inherits(u"text/csv"_s) || mime.inherits(u"text/tab-separated-values"_s)) {
        return u"csv"_s;
    }
    if (mime.inherits(u"text/vcard"_s)) {
        return u"vcard"_s;
    }
    if (mime.inherits(u"text/calendar"_s)) {
        return u"calendar"_s;
    }
    return QString();
}

// Contacts and events (kwiklook-cards.so: see cards.h).
void readCards(FileInfo &info)
{
    const int count = Cards::read(info.text.toUtf8(), info.formattedKind, FileInfo::CardLimit, &info.cards);
    info.moreCards = std::max(0, count - FileInfo::CardLimit);
}

bool isImage(const QString &mimeType)
{
    static const QList<QByteArray> types = QImageReader::supportedMimeTypes();
    return types.contains(mimeType.toLatin1());
}

QList<FileInfo::Detail> readDetails(const QString &path, const QString &mimeType, bool skipSize)
{
    // One collection per worker thread: loading the extractor plugins takes a moment.
    thread_local KFileMetaData::ExtractorCollection extractors;
    QList<FileInfo::Detail> details;
    const auto found = extractors.fetchExtractors(mimeType);
    for (KFileMetaData::Extractor *extractor : found) {
        KFileMetaData::SimpleExtractionResult result(path, mimeType, KFileMetaData::ExtractionResult::ExtractMetaData);
        extractor->extract(&result);
        const auto properties = result.properties();
        for (auto it = properties.cbegin(); it != properties.cend(); ++it) {
            if (skipSize && (it.key() == KFileMetaData::Property::Width || it.key() == KFileMetaData::Property::Height)) {
                continue; // shown as one “Dimensions” row
            }
            const KFileMetaData::PropertyInfo info(it.key());
            const QString value = info.formatAsDisplayString(it.value());
            if (!value.isEmpty() && std::none_of(details.cbegin(), details.cend(), [&](const FileInfo::Detail &d) {
                    return d.label == info.displayName();
                })) {
                details.append({info.displayName(), value});
            }
        }
        if (!details.isEmpty()) {
            break; // the first extractor that knows the type is enough
        }
    }
    return details;
}
} // namespace

FileInfo FileInfo::read(const QUrl &url)
{
    FileInfo info;
    info.url = url;
    info.known = true;
    const QMimeDatabase db;
    QMimeType mime;
    if (url.isLocalFile()) {
        const QFileInfo fi(url.toLocalFile());
        info.exists = fi.exists();
        info.isDir = fi.isDir();
        if (info.exists) {
            info.modified = fi.lastModified();
            info.created = fi.birthTime();
            info.owner = fi.owner();
            info.group = fi.group();
            info.permissions = fi.permissions();
            if (!info.isDir) {
                info.size = fi.size();
            }
        }
        mime = info.exists ? db.mimeTypeForFile(fi) : db.mimeTypeForFile(fi.fileName(), QMimeDatabase::MatchExtension);
    } else {
        info.exists = true;
        mime = db.mimeTypeForUrl(url);
    }
    info.mimeType = mime.name();
    info.typeName = mime.comment();
    info.iconName = mime.iconName();
    info.isText = mime.inherits(u"text/plain"_s);
    info.formattedKind = formattedKindOf(mime);
    if ((info.formattedKind == u"vcard"_s || info.formattedKind == u"calendar"_s) && !Cards::available()) {
        info.formattedKind.clear(); // no card library (KContacts, KCalendarCore): plain text
    }
    if (url.isLocalFile() && info.exists && !info.isDir) {
        const QString path = url.toLocalFile();
        if (isImage(info.mimeType) && info.mimeType != u"application/pdf"_s) {
            QImageReader reader(path);
            reader.setAutoTransform(true);
            info.imageSize = reader.size(); // reads the header only
            if (reader.transformation() & QImageIOHandler::TransformationRotate90) {
                info.imageSize.transpose();
            }
            info.animated = reader.supportsAnimation() && reader.imageCount() > 1;
        }
        info.details = readDetails(path, info.mimeType, info.imageSize.isValid());
        if (info.isText) {
            info.text = readTextStart(path, TextLimit, &info.textTruncated);
        }
        if (info.formattedKind == u"json"_s && !info.textTruncated) {
            const QJsonDocument json = QJsonDocument::fromJson(info.text.toUtf8());
            if (!json.isNull()) {
                info.formattedText = QString::fromUtf8(json.toJson(QJsonDocument::Indented));
            }
        }
        if (info.formattedKind == u"csv"_s) {
            const bool tabs = QMimeDatabase().mimeTypeForName(info.mimeType).inherits(u"text/tab-separated-values"_s);
            info.tableRows = parseDelimited(info.text, tabs ? QChar(u'\t') : QChar());
        }
        if (info.formattedKind == u"vcard"_s || info.formattedKind == u"calendar"_s) {
            readCards(info);
        }
        if (info.formattedKind == u"desktop"_s) {
            const KDesktopFile desktop(path); // an absolute path: never resolved into standard folders
            const KConfigGroup group = desktop.desktopGroup();
            info.desktopEntry = {
                {u"name"_s, desktop.readName()},
                {u"genericName"_s, desktop.readGenericName()},
                {u"comment"_s, desktop.readComment()},
                {u"icon"_s, desktop.readIcon()},
                {u"type"_s, desktop.readType()},
                {u"exec"_s, group.readEntry("Exec")},
                {u"url"_s, desktop.readUrl()},
                {u"categories"_s, group.readXdgListEntry("Categories").join(u", "_s)},
                {u"terminal"_s, group.readEntry("Terminal", false)},
                {u"hidden"_s, desktop.noDisplay()},
            };
        }
        if (isFont(info.mimeType) && info.size <= FontLimit) {
            QFile file(path);
            if (file.open(QIODevice::ReadOnly)) {
                info.fontData = file.readAll();
            }
        }
    }
    return info;
}

QString readTextStart(const QString &path, qint64 limit, bool *truncated)
{
    QFile file(path);
    *truncated = false;
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    QByteArray data = file.read(limit + 1);
    *truncated = data.size() > limit;
    if (*truncated) {
        data.truncate(limit);
    }
    // A byte order mark names the encoding (UTF-16, UTF-32, UTF-8); without one, UTF-8.
    const auto encoding = QStringConverter::encodingForData(data).value_or(QStringConverter::Utf8);
    // Not stateless: a character cut at the limit stays in the decoder instead of
    // counting as an error.
    QStringDecoder decoder(encoding);
    QString text = decoder.decode(data);
    if (decoder.hasError() && encoding == QStringConverter::Utf8) {
        text = QString::fromLatin1(data);
    }
    if (*truncated) {
        const qsizetype lineEnd = text.lastIndexOf(u'\n');
        if (lineEnd > 0) {
            text.truncate(lineEnd + 1);
        }
    }
    return text;
}

QList<QStringList> parseDelimited(const QString &text, QChar separator)
{
    if (separator.isNull()) {
        const QStringView firstLine = QStringView(text).left(text.indexOf(u'\n'));
        separator = u',';
        qsizetype best = firstLine.count(u',');
        for (const QChar candidate : {QChar(u';'), QChar(u'\t')}) {
            const qsizetype n = firstLine.count(candidate);
            if (n > best) {
                best = n;
                separator = candidate;
            }
        }
    }
    QList<QStringList> rows;
    QStringList row;
    QString field;
    bool quoted = false;
    bool rowStarted = false;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (quoted) {
            if (c == u'"') {
                if (i + 1 < text.size() && text.at(i + 1) == u'"') {
                    field += u'"'; // a doubled quote
                    ++i;
                } else {
                    quoted = false;
                }
            } else {
                field += c;
            }
            continue;
        }
        if (c == u'"' && field.isEmpty()) {
            quoted = true;
            rowStarted = true;
        } else if (c == separator) {
            row << field;
            field.clear();
            rowStarted = true;
        } else if (c == u'\n' || c == u'\r') {
            if (c == u'\r' && i + 1 < text.size() && text.at(i + 1) == u'\n') {
                ++i;
            }
            if (rowStarted || !field.isEmpty()) {
                row << field;
                rows << row;
            }
            row.clear();
            field.clear();
            rowStarted = false;
        } else {
            field += c;
            rowStarted = true;
        }
    }
    if (rowStarted || !field.isEmpty()) {
        row << field;
        rows << row;
    }
    return rows;
}

bool isFont(const QString &mimeType)
{
    static const QStringList types = {
        u"font/ttf"_s,
        u"font/otf"_s,
        u"font/collection"_s,
        u"font/woff"_s,
        u"font/woff2"_s,
        u"application/x-font-type1"_s,
        u"application/x-font-pcf"_s,
        u"application/x-font-bdf"_s,
    };
    return types.contains(mimeType);
}

bool isOfficeDocument(const QString &mimeType)
{
    static const QStringList prefixes{
        u"application/vnd.oasis.opendocument."_s, // ODT, ODS, ODP, ODG (and templates)
        u"application/vnd.openxmlformats-officedocument."_s, // DOCX, XLSX, PPTX
        u"application/vnd.ms-word"_s,
        u"application/vnd.ms-excel"_s,
        u"application/vnd.ms-powerpoint"_s,
    };
    if (mimeType == u"application/msword"_s) {
        return true;
    }
    return std::any_of(prefixes.begin(), prefixes.end(), [&mimeType](const QString &prefix) {
        return mimeType.startsWith(prefix);
    });
}

QString viewFor(const FileInfo &info, bool formatted)
{
    if (!info.formattedKind.isEmpty() && info.exists && !info.isDir) {
        if (!formatted && info.isText) {
            return u"TextView"_s; // the source
        }
        if (info.formattedKind == u"desktop"_s) {
            return u"DesktopView"_s;
        }
        if (info.formattedKind == u"csv"_s) {
            return u"CsvView"_s;
        }
        if (info.formattedKind == u"vcard"_s || info.formattedKind == u"calendar"_s) {
            return u"CardsView"_s;
        }
        // Markdown, HTML and JSON show formatted in the text view; SVG as a picture below.
    }
    // Pictures, text and PDF have their own views; everything else, and whatever a
    // view can't open, shows the fallback.
    if (!info.exists || info.isDir) {
        return u"FallbackView"_s;
    }
    // PDF first: Qt's image reader claims PDFs too (Qt PDF's image plugin).
    if (info.mimeType == u"application/pdf"_s) {
        return u"PdfView"_s;
    }
    if (isImage(info.mimeType)) {
        return u"ImageView"_s;
    }
    if (info.isText) {
        return u"TextView"_s;
    }
    if (!info.fontData.isEmpty()) {
        return u"FontView"_s; // Viewer turns it into the fallback if Qt can't load the font
    }
    if (ArchiveListing::isArchive(info.mimeType)) {
        return u"ArchiveView"_s;
    }
    // After text: playlists (audio/x-mpegurl) are text files and read best as such.
    if (info.mimeType.startsWith(u"video/"_s) || info.mimeType.startsWith(u"audio/"_s)) {
        return u"MediaView"_s;
    }
    return u"FallbackView"_s;
}
