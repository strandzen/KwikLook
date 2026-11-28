// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "archivelisting.h"

#include <QSet>

using namespace Qt::StringLiterals;

namespace
{
QString unescaped(const QByteArray &field)
{
    QByteArray out;
    out.reserve(field.size());
    for (qsizetype i = 0; i < field.size(); ++i) {
        const char c = field.at(i);
        if (c == '\\' && i + 1 < field.size()) {
            const char next = field.at(++i);
            out += next == 't' ? '\t' : next == 'n' ? '\n' : next == 'r' ? '\r' : next;
        } else {
            out += c;
        }
    }
    return QString::fromUtf8(out);
}
}

namespace ArchiveListing
{
bool isArchive(const QString &mimeType)
{
    static const QSet<QString> types = {
        // Archives
        u"application/zip"_s,
        u"application/x-7z-compressed"_s,
        u"application/vnd.rar"_s,
        u"application/x-tar"_s,
        u"application/x-compressed-tar"_s,
        u"application/x-bzip2-compressed-tar"_s,
        u"application/x-xz-compressed-tar"_s,
        u"application/x-zstd-compressed-tar"_s,
        u"application/x-lzma-compressed-tar"_s,
        u"application/x-lz4-compressed-tar"_s,
        u"application/x-lzip-compressed-tar"_s,
        u"application/x-tarz"_s,
        u"application/x-cpio"_s,
        u"application/vnd.efi.iso"_s, // also what x-cd-image and x-iso9660-image are now
        u"application/x-archive"_s,
        u"application/vnd.debian.binary-package"_s,
        u"application/x-rpm"_s,
        u"application/vnd.ms-cab-compressed"_s,
        u"application/x-lha"_s,
        u"application/x-xar"_s,
        u"application/vnd.comicbook+zip"_s,
        u"application/vnd.comicbook-rar"_s,
        u"application/java-archive"_s,
        u"application/vnd.android.package-archive"_s,
        // One compressed file
        u"application/gzip"_s,
        u"application/x-bzip2"_s,
        u"application/x-xz"_s,
        u"application/x-lzma"_s,
        u"application/zstd"_s,
        u"application/x-compress"_s,
        u"application/x-lz4"_s,
        u"application/x-lzip"_s,
    };
    return types.contains(mimeType);
}

Listing parse(const QByteArray &output)
{
    Listing listing;
    for (const QByteArray &line : output.split('\n')) {
        const QList<QByteArray> fields = line.split('\t');
        if (fields.value(0) == "e" && fields.size() >= 6) {
            Entry entry;
            const QByteArray kind = fields.at(1);
            entry.kind = kind == "d" ? Kind::Folder : kind == "l" ? Kind::Link : Kind::File;
            entry.size = fields.at(2).toLongLong();
            entry.modified = fields.at(3).toLongLong();
            entry.encrypted = fields.at(4) == "1";
            // The path is last and may hold (escaped) tabs: everything after the fifth tab.
            qsizetype start = 0;
            for (int i = 0; i < 5; ++i) {
                start = line.indexOf('\t', start) + 1;
            }
            entry.path = unescaped(line.mid(start));
            listing.entries.append(entry);
        } else if (fields.value(0) == "end" && fields.size() >= 4) {
            listing.total = fields.at(1).toLongLong();
            listing.unpacked = fields.at(2).toLongLong();
            listing.status = QString::fromUtf8(fields.at(3));
            listing.message = QString::fromUtf8(fields.value(4)).trimmed();
        }
    }
    return listing;
}

QStringList parts(const QString &path)
{
    QString p = path;
    while (p.startsWith(u"./"_s)) {
        p.remove(0, 2);
    }
    if (p.startsWith(u'/') || p == u".."_s || p.startsWith(u"../"_s) || p.contains(u"/../"_s) || p.endsWith(u"/.."_s)) {
        return {path}; // leaves the archive: one name, as written
    }
    QStringList out;
    for (const QString &part : p.split(u'/', Qt::SkipEmptyParts)) {
        if (part != u"."_s) {
            out.append(part);
        }
    }
    return out;
}
}
