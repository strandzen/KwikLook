// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
// The archive lister's output read back, and the folder tree's path parts.
#include "archivelisting.h"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    const ArchiveListing::Listing listing = ArchiveListing::parse(QByteArray::fromRawData(reinterpret_cast<const char *>(data), qsizetype(size)));
    for (const auto &entry : listing.entries) {
        (void)ArchiveListing::parts(entry.path);
    }
    return 0;
}
