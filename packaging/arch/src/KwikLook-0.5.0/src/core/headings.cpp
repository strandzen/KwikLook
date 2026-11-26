// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "headings.h"

#include <QRegularExpression>
#include <QTextBlock>
#include <QTextDocument>

using namespace Qt::StringLiterals;

namespace
{
// Letters and digits only, lower case: "**Lists**" and "Lists" are the same heading.
QString normalized(const QString &title)
{
    QString out;
    out.reserve(title.size());
    for (const QChar c : title) {
        if (c.isLetterOrNumber()) {
            out += c.toLower();
        }
    }
    return out;
}

QList<Headings::Heading> markdownHeadings(const QStringList &lines)
{
    static const QRegularExpression fence(u"^ {0,3}(`{3,}|~{3,})"_s);
    static const QRegularExpression atx(u"^ {0,3}(#{1,6})(?:[ \\t]+(.*?))?(?:[ \\t]+#+)?[ \\t]*$"_s);
    static const QRegularExpression underline(u"^ {0,3}(=+|-+)[ \\t]*$"_s);
    // Lines that start something else than a paragraph: an underline after them
    // is a rule or part of a list, not a heading.
    static const QRegularExpression notParagraph(u"^(?: {0,3}(?:[-*+]|\\d{1,9}[.)])(?:[ \\t]|$)| {0,3}>| {4}|\\t| {0,3}<)"_s);
    static const QRegularExpression rule(u"^ {0,3}([-*_])(?:[ \\t]*\\1){2,}[ \\t]*$"_s); // ---, * * *, ___

    QList<Headings::Heading> found;
    QString openFence; // the fence of the code block we're in
    int paragraphStart = -1; // first line of the paragraph just above, -1: none
    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines.at(i);
        if (!openFence.isEmpty()) {
            const auto m = fence.match(line);
            if (m.hasMatch() && m.captured(1).at(0) == openFence.at(0) && m.captured(1).size() >= openFence.size()
                && line.trimmed().size() == m.captured(1).size()) {
                openFence.clear();
            }
            continue;
        }
        if (const auto m = fence.match(line); m.hasMatch()) {
            openFence = m.captured(1);
            paragraphStart = -1;
            continue;
        }
        if (line.trimmed().isEmpty()) {
            paragraphStart = -1;
            continue;
        }
        if (const auto m = atx.match(line); m.hasMatch()) {
            found.append({m.captured(2).trimmed(), int(m.captured(1).size()), i});
            paragraphStart = -1;
            continue;
        }
        if (paragraphStart >= 0) {
            if (const auto m = underline.match(line); m.hasMatch()) {
                QStringList text;
                for (int j = paragraphStart; j < i; ++j) {
                    text.append(lines.at(j).trimmed());
                }
                found.append({text.join(u' '), m.captured(1).startsWith(u'=') ? 1 : 2, paragraphStart});
                paragraphStart = -1;
                continue;
            }
        }
        if (notParagraph.match(line).hasMatch() || rule.match(line).hasMatch()) {
            paragraphStart = -1;
        } else if (paragraphStart < 0) {
            paragraphStart = i;
        }
    }
    return found;
}

QList<Headings::Heading> htmlHeadings(const QString &text)
{
    static const QRegularExpression heading(u"<h([1-6])(?:\\s[^>]*)?>(.*?)</h\\1\\s*>"_s,
                                            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression comment(u"<!--.*?-->"_s, QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression tag(u"<[^>]*>"_s);
    // Comments blanked out (same length, line breaks kept): their headings don't count.
    QString source = text;
    for (auto it = comment.globalMatch(text); it.hasNext();) {
        const auto m = it.next();
        for (qsizetype i = m.capturedStart(); i < m.capturedEnd(); ++i) {
            if (source.at(i) != u'\n') {
                source[i] = u' ';
            }
        }
    }
    QList<Headings::Heading> found;
    qsizetype counted = 0;
    int line = 0;
    for (auto it = heading.globalMatch(source); it.hasNext();) {
        const auto m = it.next();
        line += source.mid(counted, m.capturedStart() - counted).count(u'\n');
        counted = m.capturedStart();
        QString title = m.captured(2);
        title.remove(tag);
        found.append({title.simplified(), m.captured(1).toInt(), line});
    }
    return found;
}
}

namespace Headings
{
QList<Heading> fromDocument(const QTextDocument &document)
{
    QList<Heading> found;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        const int level = block.blockFormat().headingLevel();
        if (level > 0) {
            found.append({block.text().simplified(), level, block.position()});
        }
    }
    return found;
}

QList<Heading> fromSource(const QString &text, const QString &kind)
{
    if (kind == u"markdown") {
        QStringList lines = text.split(u'\n');
        for (QString &line : lines) {
            if (line.endsWith(u'\r')) {
                line.chop(1);
            }
        }
        return markdownHeadings(lines);
    }
    if (kind == u"html") {
        return htmlHeadings(text);
    }
    return {};
}

QList<Heading> match(const QList<Heading> &rendered, const QList<Heading> &source)
{
    QList<Heading> out = rendered;
    if (rendered.size() == source.size()) {
        for (qsizetype i = 0; i < out.size(); ++i) {
            out[i].at = source.at(i).at;
        }
        return out;
    }
    qsizetype next = 0; // source headings before this one are taken
    int line = 0;
    for (Heading &heading : out) {
        const QString title = normalized(heading.title);
        for (qsizetype j = next; j < source.size(); ++j) {
            if (normalized(source.at(j).title) == title) {
                line = source.at(j).at;
                next = j + 1;
                break;
            }
        }
        heading.at = line;
    }
    return out;
}
}
