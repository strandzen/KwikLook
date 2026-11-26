// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QtQml/qqmlregistration.h>

#include <QQuickTextDocument>

/*
 * The entries of the contents sidebar, as plain lists for ContentsList.qml.
 *
 *   PDFs:            PdfContents { source: url } → entries [{title, level, page, y}]
 *   Markdown / HTML: Contents.fromText(area.textDocument, kind, rendered, source)
 *                    → [{title, level, position}]
 */
class Contents : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    // The headings of a TextEdit's document. Rendered: its heading paragraphs. Source
    // (`rendered` false, the TextEdit shows `source` as plain text): the headings as
    // rendered, at their lines in the source.
    Q_INVOKABLE QVariantList fromText(QQuickTextDocument *document, const QString &kind, bool rendered, const QString &source) const;

    // Also for TextView: the code blocks and quotes of a rendered document, [{kind
    // ("code" / "quote"), start, end, level}], with code padded by `padding` (see
    // TextBlocks::padCode()) for the shaded box drawn under it.
    Q_INVOKABLE QVariantList blocks(QQuickTextDocument *document, qreal padding) const;
};

/*
 * A PDF's bookmarks, depth first; `y` in page points from the top. Read off the main
 * thread (the Ghostscript manual's 1,182 took 245 ms) from a document of its own, not
 * QML's PdfBookmarkModel: that one corrupted memory when the engine deleted its
 * document first (2026-11-24). Empty until read, and for damaged or locked PDFs.
 */
class PdfContents : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)

public:
    using QObject::QObject;

    QUrl source() const;
    void setSource(const QUrl &source);
    QVariantList entries() const;

    // The reading itself (blocking).
    static QVariantList read(const QUrl &url);

Q_SIGNALS:
    void sourceChanged();
    void entriesChanged();

private:
    QUrl m_source;
    QVariantList m_entries;
    int m_generation = 0; // results of an earlier source are dropped
};
