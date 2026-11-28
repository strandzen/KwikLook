// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "markdown.h"

#include <QTest>
#include <QTextBlock>
#include <QTextDocument>

using namespace Qt::StringLiterals;

// Markdown for Qt's reader: empty HTML elements closed, code left alone.
class TstMarkdown : public QObject
{
    Q_OBJECT

    static QStringList headings(const QString &markdown)
    {
        QTextDocument doc;
        doc.setMarkdown(markdown);
        QStringList found;
        for (QTextBlock block = doc.begin(); block.isValid(); block = block.next()) {
            if (block.blockFormat().headingLevel() > 0) {
                found << block.text();
            }
        }
        return found;
    }

private Q_SLOTS:
    void closesEmptyElements_data()
    {
        QTest::addColumn<QString>("in");
        QTest::addColumn<QString>("out");
        QTest::newRow("img in an HTML block")
            << u"<div align=\"center\">\n    <img width=\"225\" src=\"assets/icon.svg\">\n    <h1>KwikLook</h1>\n</div>"_s
            << u"<div align=\"center\">\n    <img width=\"225\" src=\"assets/icon.svg\"/>\n    <h1>KwikLook</h1>\n</div>"_s;
        QTest::newRow("br and hr") << u"one<br>two\n\n<HR>"_s << u"one<br/>two\n\n<HR/>"_s;
        QTest::newRow("> in a quoted value") << u"a <img alt=\"x > y\" src='p.png'> b"_s << u"a <img alt=\"x > y\" src='p.png'/> b"_s;
        QTest::newRow("already closed") << u"<img src=\"a.png\" /> <br/>"_s << u"<img src=\"a.png\" /> <br/>"_s;
        QTest::newRow("not empty elements") << u"<b>bold</b> <imgx> <brand>"_s << u"<b>bold</b> <imgx> <brand>"_s;
        QTest::newRow("inline code") << u"Use `<br>` or ``a `<img>` b``, then <br>"_s << u"Use `<br>` or ``a `<img>` b``, then <br/>"_s;
        QTest::newRow("fenced code") << u"```html\n<img src=\"a.png\">\n```\n<br>"_s << u"```html\n<img src=\"a.png\">\n```\n<br/>"_s;
        QTest::newRow("tilde fence") << u"~~~~\n<br>\n~~~\n<br>\n~~~~\n<br>"_s << u"~~~~\n<br>\n~~~\n<br>\n~~~~\n<br/>"_s;
        QTest::newRow("indented code") << u"Text.\n\n    <br>\n\n<br>"_s << u"Text.\n\n    <br>\n\n<br/>"_s;
        QTest::newRow("indented, not code: in a paragraph") << u"Text\n    <br>"_s << u"Text\n    <br/>"_s;
    }
    void closesEmptyElements()
    {
        QFETCH(QString, in);
        QFETCH(QString, out);
        QCOMPARE(Markdown::forQt(in), out);
        QCOMPARE(Markdown::forQt(in).count(u'\n'), in.count(u'\n')); // same lines
    }

    // What it's for: Qt's reader keeps the text after the picture.
    void qtKeepsTheTextAfterPictures()
    {
        const QString readme = u"<div align=\"center\">\n    <img src=\"icon.svg\">\n    <h1><b>KwikLook</b></h1>\n</div>\n\nIntro.\n\n## Screenshots\n\nText.\n"_s;
        QCOMPARE(headings(Markdown::forQt(readme)), (QStringList{u"KwikLook"_s, u"Screenshots"_s}));
        if (headings(readme).contains(u"Screenshots"_s)) {
            qInfo("Qt's Markdown reader keeps text after <img> now: Markdown::forQt may no longer be needed");
        }
    }
};

QTEST_GUILESS_MAIN(TstMarkdown)
#include "tst_markdown.moc"
