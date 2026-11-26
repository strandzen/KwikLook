// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
// CSV / TSV: the separator guessed, and given.
#include "fileinfo.h"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    const QString text = QString::fromUtf8(reinterpret_cast<const char *>(data), qsizetype(size));
    (void)parseDelimited(text);
    (void)parseDelimited(text, QChar(u'\t'));
    return 0;
}
