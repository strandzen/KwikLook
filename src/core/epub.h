// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVariantList>

// EPUB books for the preview: the book's files, as kwiklook-archive --files hands
// them over (unpacked in the sandbox), become one HTML document for Qt's rich text:
// the cover, title and author, then the chapters in reading order. Only harmless
// formatting is kept (headings, paragraphs, emphasis, lists, tables, quotes, code,
// pictures); scripts, styles, frames, forms and links inside the book are dropped,
// pictures become data: addresses (nothing is fetched). Pure Qt, no I/O.
namespace Epub
{
// The helper's output, read: the files by path in the book, and how it ended.
struct Files {
    QHash<QString, QByteArray> files;
    QString status; // "ok", "limit", "time", "error"; empty if the output had no end line
    QString message;
};
Files parseHelperOutput(const QByteArray &output);

struct Book {
    bool ok = false; // a book could be read (container, package, at least one chapter)
    QString title;
    QString author;
    QString html; // the whole book as one document
    bool truncated = false; // only the start (see TextLimit), or the helper stopped early
    // The book's own table of contents: {title, level (1 = top), anchor}, anchor being
    // an <a name> in `html`; empty if the book has none.
    QVariantList contents;
};

// Text beyond this (characters, without markup and pictures) is left out: long books
// stay quick to lay out. Measured 2026-11-26 (Qt 6.12, 720 px wide): rich text takes
// about 0.2 s per MB of HTML to read and lay out (1 MB 194 ms, 2 MB 394 ms); 1 MB of
// text is most novels whole.
constexpr qsizetype TextLimit = 1024 * 1024;
// Pictures beyond this in all (bytes as stored) are left out.
constexpr qsizetype PictureLimit = 24 * 1024 * 1024;
// Pictures are shown at most this wide (logical pixels): the reading column.
constexpr int PictureWidth = 600;

// The files kwiklook-archive should hand over (name endings).
QStringList wantedSuffixes();

Book build(const Files &files);
}
