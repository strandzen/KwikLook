// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QSize>
#include <QString>

// Copies a photo's metadata (Exif: date taken, camera, location, ...; IPTC; XMP) from
// `from` to the edited picture `to`, with exiv2: the orientation set to normal (the
// pixels are turned already), the embedded thumbnail dropped, the pixel size set to
// `size`. False when either file's type can't hold metadata or exiv2 fails: the
// picture itself is fine then, only without metadata. Exceptions stay in this file.
bool copyImageMetadata(const QString &from, const QString &to, QSize size);
