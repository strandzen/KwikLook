// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QList>

class QTextDocument;

/*
 * Code blocks and quotes of rendered Markdown and HTML, for TextView to draw a shaded
 * box under code and a bar beside quotes (Qt's text layout draws neither).
 *
 * Qt's Markdown reader (6.12) makes each line of a fenced code block its own paragraph,
 * marked with BlockCodeFence and BlockCodeLanguage, with nothing between two fences in
 * a row: a change of language starts a new box. Indented code has BlockCodeLanguage
 * (empty) without the fence; HTML's <pre> has non-breaking lines. Quotes carry
 * BlockQuoteLevel and a 40 px margin per level.
 */
namespace TextBlocks
{
enum class Kind { Code, Quote };

struct Run {
    Kind kind = Kind::Code;
    int start = 0; // position of the first block
    int end = 0; // end of the last block's text
    int level = 0; // quotes: 1 = outermost (a nested quote gives a run per level)
};

QList<Run> find(const QTextDocument &document);

// Gives code runs room inside their box: `padding` on the sides, 2.5 times that above
// and below (the box reaches `padding` past the text). Changes nothing when already done,
// so it can run on every update.
void padCode(QTextDocument &document, const QList<Run> &runs, qreal padding);
}
