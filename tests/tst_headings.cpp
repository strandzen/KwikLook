// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "headings.h"
#include "textblocks.h"

#include <QSignalSpy>

#include <QTest>
#include <QTextBlock>
#include <QTextDocument>

using namespace Qt::StringLiterals;

namespace
{
QStringList titles(const QList<Headings::Heading> &list)
{
    QStringList out;
    for (const auto &h : list) {
        out.append(h.title);
    }
    return out;
}
QList<int> levels(const QList<Headings::Heading> &list)
{
    QList<int> out;
    for (const auto &h : list) {
        out.append(h.level);
    }
    return out;
}
QList<int> ats(const QList<Headings::Heading> &list)
{
    QList<int> out;
    for (const auto &h : list) {
        out.append(h.at);
    }
    return out;
}

const QString Markdown = u"# Title #\n"                  // 0
                         "Intro text\n"                  // 1
                         "\n"                            // 2
                         "Underlined one\n"              // 3
                         "==============\n"              // 4
                         "```\n"                         // 5
                         "# not a heading (code)\n"      // 6
                         "```\n"                         // 7
                         "#hashtag is not one\n"         // 8
                         "\n"                            // 9
                         "- a list item\n"               // 10
                         "---\n"                         // 11 (a rule after a list)
                         "Two lines of\n"                // 12
                         "a heading\n"                   // 13
                         "---\n"                         // 14
                         "   ### Indented **bold**\n"    // 15
                         "    # four spaces: code\n"     // 16
                         "~~~~\n"                        // 17
                         "## in a tilde fence\n"         // 18
                         "~~~~\n"                        // 19
                         "###### Six\n"_s;               // 20
}

class TstHeadings : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void markdownSource()
    {
        const auto found = Headings::fromSource(Markdown, u"markdown"_s);
        QCOMPARE(titles(found), (QStringList{u"Title"_s, u"Underlined one"_s, u"Two lines of a heading"_s, u"Indented **bold**"_s, u"Six"_s}));
        QCOMPARE(levels(found), (QList<int>{1, 1, 2, 3, 6}));
        QCOMPARE(ats(found), (QList<int>{0, 3, 12, 15, 20}));
    }

    void windowsLineEndings()
    {
        QString crlf = Markdown;
        crlf.replace(u"\n"_s, u"\r\n"_s);
        QCOMPARE(ats(Headings::fromSource(crlf, u"markdown"_s)), (QList<int>{0, 3, 12, 15, 20}));
    }

    // What Qt's Markdown reader calls headings agrees with the scan.
    void markdownRenderedAgrees()
    {
        QTextDocument doc;
        doc.setMarkdown(Markdown);
        const auto rendered = Headings::fromDocument(doc);
        QCOMPARE(titles(rendered), (QStringList{u"Title"_s, u"Underlined one"_s, u"Two lines of a heading"_s, u"Indented bold"_s, u"Six"_s}));
        QCOMPARE(levels(rendered), (QList<int>{1, 1, 2, 3, 6}));
        for (const auto &h : rendered) {
            QVERIFY(doc.findBlock(h.at).blockFormat().headingLevel() > 0);
        }
    }

    void htmlSourceAndRendered()
    {
        const QString html = u"<html><body>\n"
                             "<h1>First</h1>\n"
                             "<!-- <h2>commented out</h2>\n"
                             "-->\n"
                             "<p>text</p><H2 class=\"x\">Second <b>bold</b></H2>\n"
                             "<h3>\nThird\n</h3>\n"
                             "</body></html>\n"_s;
        const auto source = Headings::fromSource(html, u"html"_s);
        QCOMPARE(titles(source), (QStringList{u"First"_s, u"Second bold"_s, u"Third"_s}));
        QCOMPARE(levels(source), (QList<int>{1, 2, 3}));
        QCOMPARE(ats(source), (QList<int>{1, 4, 5}));
        // Qt's HTML reader marks <h1>-<h6> too.
        QTextDocument doc;
        doc.setHtml(html);
        QCOMPARE(titles(Headings::fromDocument(doc)), (QStringList{u"First"_s, u"Second bold"_s, u"Third"_s}));
    }

    void matchByOrderOrText()
    {
        const QList<Headings::Heading> rendered{{u"A"_s, 1, 100}, {u"B c"_s, 2, 200}, {u"D"_s, 2, 300}};
        // Same count: by order, whatever the titles.
        QCOMPARE(ats(Headings::match(rendered, {{u"x"_s, 1, 1}, {u"y"_s, 1, 5}, {u"z"_s, 1, 9}})), (QList<int>{1, 5, 9}));
        // Different count: by text (ignoring case and marks); no match keeps the line before.
        const QList<Headings::Heading> source{{u"a"_s, 1, 2}, {u"extra"_s, 1, 4}, {u"**B** c"_s, 2, 7}, {u"more"_s, 1, 9}};
        const auto matched = Headings::match(rendered, source);
        QCOMPARE(ats(matched), (QList<int>{2, 7, 7}));
        QCOMPARE(titles(matched), titles(rendered));
    }

    void otherKindsHaveNone()
    {
        QVERIFY(Headings::fromSource(u"# x\n"_s, u"json"_s).isEmpty());
        QVERIFY(Headings::fromSource(QString(), u"markdown"_s).isEmpty());
    }

    // Code blocks (split where the language changes) and quotes (a run per level).
    void findsCodeAndQuotes()
    {
        QTextDocument d;
        d.setMarkdown(u"Text\n\n```cpp\na\nb\n```\n```py\nc\n```\n\n    ind\n\n> q\n>\n> > n\n\nafter\n"_s);
        const QList<TextBlocks::Run> runs = TextBlocks::find(d);
        QStringList got;
        for (const auto &r : runs) {
            got << u"%1 %2-%3 %4"_s.arg(r.kind == TextBlocks::Kind::Code ? u"code"_s : u"quote"_s, d.findBlock(r.start).text(), d.findBlock(r.end).text())
                       .arg(r.level);
        }
        QCOMPARE(got, (QStringList{u"code a-b 0"_s, u"code c-c 0"_s, u"code ind-ind 0"_s, u"quote n-n 2"_s, u"quote q-n 1"_s}));
        QCOMPARE(runs.first().end, d.findBlock(runs.first().end).position() + 1); // end of "b"

        // HTML: <pre> and <blockquote>.
        QTextDocument h;
        h.setHtml(u"<p>x</p><pre>one\ntwo</pre><blockquote><p>q</p></blockquote>"_s);
        QStringList html;
        for (const auto &r : TextBlocks::find(h)) {
            html << h.findBlock(r.start).text() + u'-' + h.findBlock(r.end).text();
        }
        QCOMPARE(html, (QStringList{u"one-two"_s, u"q-q"_s}));

        // Padding once; a second pass changes nothing (it runs on every update).
        TextBlocks::padCode(d, runs, 8);
        QCOMPARE(d.findBlock(runs.first().start).blockFormat().topMargin(), 20.0);
        QCOMPARE(d.findBlock(runs.first().start).blockFormat().leftMargin(), 8.0);
        QCOMPARE(d.findBlock(runs.first().start).blockFormat().bottomMargin(), 0.0);
        QCOMPARE(d.findBlock(runs.first().end).blockFormat().bottomMargin(), 20.0);
        QSignalSpy changed(&d, &QTextDocument::contentsChanged);
        TextBlocks::padCode(d, TextBlocks::find(d), 8);
        QCOMPARE(changed.count(), 0);
    }
};

QTEST_GUILESS_MAIN(TstHeadings)
#include "tst_headings.moc"
