// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
// What reads files inside the viewer itself, not in the sandbox: FileInfo::read (type,
// KFileMetaData's details, text, fonts, cards) and the picture decode ImageView does.
// The first byte picks the file name's extension; the rest is the file.
#include "fileinfo.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImageReader>
#include <QStandardPaths>
#include <QUrl>

#include <cstddef>
#include <cstdint>

namespace
{
QString s_dir;
}

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QStandardPaths::setTestModeEnabled(true);
    new QGuiApplication(*argc, *argv); // image and font plugins need it
    s_dir = QDir::tempPath() + QStringLiteral("/kwiklook-fuzz-%1").arg(QCoreApplication::applicationPid());
    QDir().mkpath(s_dir);
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static const char *const extensions[] = {"jpg", "png", "gif", "webp", "svg", "tiff", "bmp", "ico", "avif", "jxl", "psd", "tga", "pdf",
                                             "mp3", "flac", "ogg", "mp4", "mkv", "ttf", "otf", "md", "html", "json", "csv", "desktop",
                                             "vcf", "ics", "txt", "docx", "odt", "xlsx", "zip", "dng", "heic", "exr", "qoi"};
    if (size < 1 || size > 2 * 1024 * 1024) {
        return 0;
    }
    const QString path = s_dir + QStringLiteral("/input.") + QLatin1String(extensions[data[0] % std::size(extensions)]);
    {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return 0;
        }
        f.write(reinterpret_cast<const char *>(data + 1), qint64(size - 1));
    }
    const FileInfo info = FileInfo::read(QUrl::fromLocalFile(path));
    if (info.imageSize.isValid()) {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        // As ImageView: never more than a screen's worth (and no huge allocations here).
        if (qint64(info.imageSize.width()) * info.imageSize.height() > 4096 * 4096) {
            reader.setScaledSize(info.imageSize.scaled(1024, 1024, Qt::KeepAspectRatio));
        }
        (void)reader.read();
    }
    return 0;
}
