// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "markdown.h"

#include <QRegularExpression>
#include <QStringList>

using namespace Qt::StringLiterals;

namespace Markdown
{
namespace
{
// An empty element without its slash; quoted attribute values may hold “>”.
const QRegularExpression &voidTag()
{
    static const QRegularExpression tag(u"<(area|base|br|col|embed|hr|img|input|link|meta|param|source|track|wbr)\\b"
                                        u"((?:[^<>\"'/]|\"[^\"]*\"|'[^']*'|/(?!>))*?)\\s*>"_s,
                                        QRegularExpression::CaseInsensitiveOption);
    return tag;
}

// One line's text outside `code spans`, with the tags closed.
QString closedOutsideCode(const QString &line)
{
    QString out;
    qsizetype at = 0;
    while (at < line.size()) {
        const qsizetype tick = line.indexOf(u'`', at);
        QString plain = line.mid(at, tick < 0 ? -1 : tick - at);
        plain.replace(voidTag(), u"<\\1\\2/>"_s);
        out += plain;
        if (tick < 0) {
            break;
        }
        // A code span: the same number of backticks closes it (CommonMark).
        qsizetype run = tick;
        while (run < line.size() && line.at(run) == u'`') {
            ++run;
        }
        const QString ticks = line.mid(tick, run - tick);
        const qsizetype close = line.indexOf(ticks, run);
        if (close < 0) {
            out += line.mid(tick); // no closing run: literal backticks, rest as it is
            break;
        }
        out += line.mid(tick, close + ticks.size() - tick);
        at = close + ticks.size();
    }
    return out;
}

int indentOf(const QString &line)
{
    int width = 0;
    for (const QChar c : line) {
        if (c == u' ') {
            ++width;
        } else if (c == u'\t') {
            width += 4 - width % 4;
        } else {
            break;
        }
    }
    return width;
}
}

QString forQt(const QString &text)
{
    if (!text.contains(u'<')) {
        return text;
    }
    const QStringList lines = text.split(u'\n');
    QStringList out;
    out.reserve(lines.size());
    QString fence; // the open fence (``` or ~~~, at least three), empty outside one
    bool previousBlank = true;
    bool inHtml = false; // an HTML block: up to the next blank line
    bool inIndentedCode = false;
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        const bool blank = trimmed.isEmpty();
        if (!fence.isEmpty()) {
            if (trimmed.startsWith(fence) && trimmed.mid(fence.size()).trimmed().remove(fence.at(0)).isEmpty()) {
                fence.clear();
            }
            out << line;
            previousBlank = blank;
            continue;
        }
        const int indent = indentOf(line);
        if (indent < 4 && (trimmed.startsWith(u"```"_s) || trimmed.startsWith(u"~~~"_s))) {
            qsizetype n = 0;
            while (n < trimmed.size() && trimmed.at(n) == trimmed.at(0)) {
                ++n;
            }
            fence = trimmed.left(n);
            out << line;
            previousBlank = false;
            continue;
        }
        if (blank) {
            inHtml = false;
            out << line;
            previousBlank = true;
            continue;
        }
        // Indented code starts after a blank line (it can't interrupt a paragraph or
        // an HTML block) and goes on while lines stay indented.
        inIndentedCode = indent >= 4 && !inHtml && (previousBlank || inIndentedCode);
        if (inIndentedCode) {
            out << line;
            previousBlank = false;
            continue;
        }
        if (previousBlank && indent < 4 && trimmed.startsWith(u'<')) {
            inHtml = true;
        }
        out << closedOutsideCode(line);
        previousBlank = false;
    }
    const QString result = out.join(u'\n');
    return result == text ? text : result;
}
}
