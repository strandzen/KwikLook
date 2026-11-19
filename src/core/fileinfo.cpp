// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "fileinfo.h"

#include <KFileMetaData/ExtractorCollection>
#include <KFileMetaData/PropertyInfo>
#include <KFileMetaData/SimpleExtractionResult>

#include <QFileInfo>
#include <QStringDecoder>
#include <QImageReader>

#include <algorithm>
#include <QMimeDatabase>

using namespace Qt::StringLiterals;

namespace
{
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

QString viewFor(const FileInfo &info)
{
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
    // After text: playlists (audio/x-mpegurl) are text files and read best as such.
    if (info.mimeType.startsWith(u"video/"_s) || info.mimeType.startsWith(u"audio/"_s)) {
        return u"MediaView"_s;
    }
    return u"FallbackView"_s;
}
