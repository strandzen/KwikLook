// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QDateTime>
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

    // Local files are checked on disk and their type found from name and content,
    // which reads the file's start: may block, run it off the main thread.
    // Remote URLs get a type from their name only.
    static FileInfo read(const QUrl &url);

    bool operator==(const FileInfo &) const = default;
};

// The QML file (component name in the kwiklook module) that previews this file.
QString viewFor(const FileInfo &info);
