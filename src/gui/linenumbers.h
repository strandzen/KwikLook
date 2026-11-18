// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickTextDocument>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

/*
 * Line numbers for a QML TextEdit/TextArea, which has none: where each line (text
 * block) starts in the laid-out document, wrapped lines included, for the visible
 * part only. QML draws `lines` in a gutter at the left.
 *
 *   LineNumbers { id: numbers; document: area.textDocument; top: flick.contentY; height: flick.height }
 *   Repeater { model: numbers.lines; Label { y: modelData.y; text: modelData.number } }
 */
class LineNumbers : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QQuickTextDocument *document READ document WRITE setDocument NOTIFY documentChanged)
    // The visible part, in document coordinates.
    Q_PROPERTY(qreal top MEMBER m_top WRITE setTop NOTIFY viewChanged)
    Q_PROPERTY(qreal height MEMBER m_height WRITE setHeight NOTIFY viewChanged)
    // [{number, y}] for the lines that start in or just around the visible part.
    Q_PROPERTY(QVariantList lines READ lines NOTIFY linesChanged)
    // Number of lines in the document (for the gutter's width).
    Q_PROPERTY(int count READ count NOTIFY linesChanged)

public:
    explicit LineNumbers(QObject *parent = nullptr);

    QQuickTextDocument *document() const;
    void setDocument(QQuickTextDocument *document);
    void setTop(qreal top);
    void setHeight(qreal height);
    QVariantList lines() const;
    int count() const;

Q_SIGNALS:
    void documentChanged();
    void viewChanged();
    void linesChanged();

private:
    void update();

    QPointer<QQuickTextDocument> m_document;
    qreal m_top = 0;
    qreal m_height = 0;
    QVariantList m_lines;
    int m_count = 0;
    QTimer m_timer; // coalesces scrolling and layout changes into one update per frame
};
