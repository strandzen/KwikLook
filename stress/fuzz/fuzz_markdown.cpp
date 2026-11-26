// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
// Markdown for Qt's reader, and the source heading scanners (Markdown and HTML).
#include "headings.h"
#include "markdown.h"

#include <QString>

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    const QString text = QString::fromUtf8(reinterpret_cast<const char *>(data), qsizetype(size));
    const QString forQt = Markdown::forQt(text);
    if (forQt.count(u'\n') != text.count(u'\n')) {
        __builtin_trap(); // it must never change the lines (the contents use them)
    }
    (void)Headings::fromSource(text, QStringLiteral("markdown"));
    (void)Headings::fromSource(text, QStringLiteral("html"));
    return 0;
}
