// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QList>
#include <QString>

class QTextDocument;

/*
 * Headings of Markdown and HTML files, for the contents sidebar.
 *
 * The list comes from the rendered document (Qt's readers mark heading paragraphs),
 * so it matches what's shown. The source view needs the line of each heading in the
 * source: sourceHeadings() scans for them, matchHeadings() pairs both lists.
 */
namespace Headings
{
struct Heading {
    QString title;
    int level = 1; // 1-6
    int at = 0; // a position in the document, or a line in the source
};

// Heading paragraphs of a rendered document; `at` is the block's position.
QList<Heading> fromDocument(const QTextDocument &document);

// Heading lines of Markdown ("markdown": # lines and underlined ones, outside code
// fences) or HTML ("html": <h1>-<h6>); `at` is the line (0-based).
QList<Heading> fromSource(const QString &text, const QString &kind);

// The rendered headings with the source line of each: by order when both lists are
// as long, else by heading text (a heading without a match takes the line of the one
// before it).
QList<Heading> match(const QList<Heading> &rendered, const QList<Heading> &source);
}
