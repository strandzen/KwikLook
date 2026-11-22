// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "imagemetadata.h"

#include <exiv2/exiv2.hpp>

#include <QFile>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcMetadata, "kwiklook.metadata")

bool copyImageMetadata(const QString &from, const QString &to, QSize size)
{
    try {
        auto source = Exiv2::ImageFactory::open(QFile::encodeName(from).toStdString());
        source->readMetadata();
        Exiv2::ExifData exif = source->exifData();
        Exiv2::XmpData xmp = source->xmpData();
        if (!exif.empty()) {
            exif["Exif.Image.Orientation"] = uint16_t(1);
            Exiv2::ExifThumb(exif).erase();
            exif["Exif.Photo.PixelXDimension"] = uint32_t(size.width());
            exif["Exif.Photo.PixelYDimension"] = uint32_t(size.height());
        }
        const auto orientation = xmp.findKey(Exiv2::XmpKey("Xmp.tiff.Orientation"));
        if (orientation != xmp.end()) {
            xmp.erase(orientation);
        }
        auto target = Exiv2::ImageFactory::open(QFile::encodeName(to).toStdString());
        target->readMetadata(); // keeps what the writer put in (ICC profile)
        target->setExifData(exif);
        target->setIptcData(source->iptcData());
        target->setXmpData(xmp);
        target->writeMetadata();
        return true;
    } catch (const Exiv2::Error &error) {
        qCInfo(lcMetadata) << "metadata not copied:" << error.what() << from << to;
        return false;
    } catch (const std::exception &error) {
        qCWarning(lcMetadata) << "metadata not copied:" << error.what();
        return false;
    }
}
