// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QString>

namespace Markdown
{
// The Markdown text as Qt's Markdown reader can take it: HTML's empty elements
// written without their slash (<img src="a.png">, <br>, <hr>) get one (<img … />).
// Checked 2026-11-26 (Qt 6.12.1): without it, Qt drops all the text after such a tag,
// and GitHub READMEs often start with <div align="center"><img …>. Code (fenced,
// indented, inline) is left as written. Only characters within lines are added, so
// line numbers stay the same. Returns `text` itself when nothing needed changing.
QString forQt(const QString &text);
}
