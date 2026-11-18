// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "fileinfo.h"

#include <QFileInfo>
#include <QMimeDatabase>

using namespace Qt::StringLiterals;

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
    return info;
}

QString viewFor(const FileInfo &info)
{
    Q_UNUSED(info)
    // Images, text and PDF get their own views in step 7; everything else, and
    // whatever a view can't open, shows the fallback.
    return u"FallbackView"_s;
}
