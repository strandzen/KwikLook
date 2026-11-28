// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QDateTime>
#include <QFile>
#include <QList>
#include <QSize>
#include <QString>
#include <QUrl>
#include <QVariantMap>

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
    bool animated = false; // pictures with more than one frame (GIF, WebP, ...)
    bool isText = false; // plain text, source code, Markdown, ... (type inherits text/plain)
    QString text; // text files: the start of the file (see TextLimit)
    bool textTruncated = false; // the file is longer than `text`
    QByteArray fontData; // font files (up to FontLimit): the whole file, for the font preview
    // Files that show formatted or as source (the bar's view toggle): "markdown",
    // "html", "svg", "json", "desktop", "csv", "vcard", "calendar"; empty for the others.
    QString formattedKind;
    // JSON: the text indented, if it parses; Markdown: the text as Qt's reader can take
    // it (Markdown::forQt), if that differs; else empty.
    QString formattedText;
    // .desktop files: name, genericName, comment, icon, type, exec, url, categories,
    // terminal, hidden (in the menus), as read by KDesktopFile.
    QVariantMap desktopEntry;
    QList<QStringList> tableRows; // CSV / TSV: the rows of `text`, split into fields
    // vCard / iCalendar: one map per contact or event (at most CardLimit), in the
    // file's order; see readCards() in fileinfo.cpp for the keys.
    QVariantList cards;
    int moreCards = 0; // contacts or events beyond CardLimit

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
    // Larger font files show the fallback view (the largest CJK fonts are about 20 MB).
    static constexpr qint64 FontLimit = 64 * 1024 * 1024;
    static constexpr int CardLimit = 200;

    bool operator==(const FileInfo &) const = default;
};

// The start of a text file: up to `limit` bytes, cut at a line end; UTF-8, or
// Latin-1 if it isn't valid UTF-8. `truncated` tells whether there was more.
QString readTextStart(const QString &path, qint64 limit, bool *truncated);

// Office documents LibreOffice can convert to PDF: OpenDocument, Office Open XML,
// and the older Microsoft Office formats.
bool isOfficeDocument(const QString &mimeType);

// Comma-, semicolon- or tab-separated text as rows of fields: quoted fields may hold
// separators, line breaks and doubled quotes (RFC 4180). `separator` 0: the one of
// ',', ';', '\t' found most often in the first line.
QList<QStringList> parseDelimited(const QString &text, QChar separator = QChar());

// Font files Qt can load (through FreeType): TrueType, OpenType, collections, WOFF,
// Type 1, PCF, BDF.
bool isFont(const QString &mimeType);

// The QML file (component name in the kwiklook module) that previews this file.
// `formatted`: for files with a formattedKind, formatted (else their source as text).
QString viewFor(const FileInfo &info, bool formatted = true);
