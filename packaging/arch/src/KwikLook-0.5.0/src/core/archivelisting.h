// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

/*
 * What kwiklook-archive prints (src/archive/main.cpp), read back: the entries it
 * listed and how the listing ended.
 */
namespace ArchiveListing
{
// Archive types listed by their contents (exact names, not what they inherit:
// office documents and EPUB are zip files too, with views of their own), and single
// compressed files (notes.txt.gz), shown as one entry.
bool isArchive(const QString &mimeType);

enum class Kind { File, Folder, Link };

struct Entry {
    QString path; // as stored: may start with "./", end with "/", or try to leave ("../x")
    Kind kind = Kind::File;
    qint64 size = -1; // -1: unknown
    qint64 modified = -1; // Unix time, -1: unknown
    bool encrypted = false;
};

struct Listing {
    QList<Entry> entries; // at most the helper's limit
    qint64 total = 0; // entries in the archive (or counted until the time was up)
    qint64 unpacked = 0; // bytes, files only
    QString status; // "ok", "time" (stopped, partial), "locked", "error"; empty: no end line
    QString message; // libarchive's, for the log
};

Listing parse(const QByteArray &output);

// The path's parts, for the folder tree: "./docs/a.txt" → {"docs", "a.txt"}. A path
// that leaves the archive ("../x", "/etc/x") stays one part, shown as written.
QStringList parts(const QString &path);
}
