// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
// Books: the helper's output (any bytes) read back, then built into the document.
#include "epub.h"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    const Epub::Files files = Epub::parseHelperOutput(QByteArray::fromRawData(reinterpret_cast<const char *>(data), qsizetype(size)));
    (void)Epub::build(files);
    return 0;
}
