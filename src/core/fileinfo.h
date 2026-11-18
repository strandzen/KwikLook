// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QDateTime>
#include <QFile>
#include <QList>
#include <QSize>
#include <QString>
#include <QUrl>

// What the preview needs to know about a file before showing it.
struct FileInfo {
    QUrl url;
    bool known = false; // read() has run
    bool exists = false; // local files only; remote ones count as existing
    bool isDir = false;
    QString mimeType; // e.g. "text/plain"
    QString typeName; // e.g. "Plain text document", in the user's language
    QString iconName;
    qint64 size = -1; // bytes; -1 for folders and unknown
    QDateTime modified; // invalid if unknown
    QDateTime created; // invalid if the file system doesn't record it
    QString owner;
    QString group;
    QFile::Permissions permissions;
    QSize imageSize; // pictures Qt can read; invalid otherwise
    bool isText = false; // plain text, source code, Markdown, ... (type inherits text/plain)
    QString text; // text files: the start of the file (see TextLimit)
    bool textTruncated = false; // the file is longer than `text`

    // What KFileMetaData finds in the file (EXIF, page count, artist, ...), as
    // translated label and display text, in its order. Width and height are left
    // out when imageSize has them.
    struct Detail {
        QString label;
        QString value;
        bool operator==(const Detail &) const = default;
    };
    QList<Detail> details;

    // Local files are checked on disk and their type found from name and content,
    // which reads the file's start: may block, run it off the main thread.
    // Remote URLs get a type from their name only. Local files also get the picture
    // size, KFileMetaData's details and the start of text files (parses the file).
    static FileInfo read(const QUrl &url);

    // Text files show at most this much: more makes the preview slow to lay out.
    // Measured 2026-11-18: 1 MB took 0.46 s to lay out and colour, 256 KB 0.11 s.
    static constexpr qint64 TextLimit = 256 * 1024;

    bool operator==(const FileInfo &) const = default;
};

// The start of a text file: up to `limit` bytes, cut at a line end; UTF-8, or
// Latin-1 if it isn't valid UTF-8. `truncated` tells whether there was more.
QString readTextStart(const QString &path, qint64 limit, bool *truncated);

// The QML file (component name in the kwiklook module) that previews this file.
QString viewFor(const FileInfo &info);
