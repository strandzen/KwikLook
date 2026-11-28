// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "textblocks.h"

#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

#include <optional>

using namespace Qt::StringLiterals;

namespace
{
// The code "language" of a block (a new one starts a new box), nothing when not code.
std::optional<QString> codeOf(const QTextBlock &block)
{
    const QTextBlockFormat format = block.blockFormat();
    if (format.hasProperty(QTextFormat::BlockCodeFence)) {
        return u"fence:"_s + format.stringProperty(QTextFormat::BlockCodeLanguage);
    }
    if (format.hasProperty(QTextFormat::BlockCodeLanguage)) {
        return u"indented"_s; // set, empty, on indented code
    }
    if (format.nonBreakableLines()) {
        return u"pre"_s;
    }
    return std::nullopt;
}

int endOf(const QTextBlock &block)
{
    return block.position() + std::max(0, block.length() - 1);
}
}

QList<TextBlocks::Run> TextBlocks::find(const QTextDocument &document)
{
    QList<Run> runs;
    std::optional<QString> code;
    QList<int> quoteStarts; // open quote runs, per level
    QTextBlock previous;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        const std::optional<QString> c = codeOf(block);
        if (c && code == c) {
            runs.last().end = endOf(block);
        } else if (c) {
            runs.append({Kind::Code, block.position(), endOf(block), 0});
        }
        code = c;

        const int level = block.blockFormat().intProperty(QTextFormat::BlockQuoteLevel);
        while (quoteStarts.size() > level) {
            runs.append({Kind::Quote, quoteStarts.takeLast(), endOf(previous), int(quoteStarts.size()) + 1});
        }
        while (quoteStarts.size() < level) {
            quoteStarts.append(block.position());
        }
        previous = block;
    }
    while (!quoteStarts.isEmpty()) {
        runs.append({Kind::Quote, quoteStarts.takeLast(), endOf(previous), int(quoteStarts.size()) + 1});
    }
    return runs;
}

void TextBlocks::padCode(QTextDocument &document, const QList<Run> &runs, qreal padding)
{
    for (const Run &run : runs) {
        if (run.kind != Kind::Code) {
            continue;
        }
        const QTextBlock first = document.findBlock(run.start);
        const QTextBlock last = document.findBlock(run.end);
        for (QTextBlock block = first; block.isValid(); block = block.next()) {
            QTextBlockFormat format = block.blockFormat();
            // Qt lays out the larger of two margins that meet, not both: room for this
            // box's padding and the next one's, and a gap.
            const qreal top = block == first ? padding * 2.5 : 0;
            const qreal bottom = block == last ? padding * 2.5 : 0;
            if (format.leftMargin() != padding || format.rightMargin() != padding || format.topMargin() != top || format.bottomMargin() != bottom) {
                format.setLeftMargin(padding);
                format.setRightMargin(padding);
                format.setTopMargin(top);
                format.setBottomMargin(bottom);
                QTextCursor(block).setBlockFormat(format);
            }
            if (block == last) {
                break;
            }
        }
    }
}
