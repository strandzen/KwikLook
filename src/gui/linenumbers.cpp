// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "linenumbers.h"

#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextDocument>

using namespace Qt::StringLiterals;

LineNumbers::LineNumbers(QObject *parent)
    : QObject(parent)
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(0);
    connect(&m_timer, &QTimer::timeout, this, &LineNumbers::update);
}

QQuickTextDocument *LineNumbers::document() const
{
    return m_document;
}

void LineNumbers::setDocument(QQuickTextDocument *document)
{
    if (document == m_document) {
        return;
    }
    if (m_document && m_document->textDocument()) {
        m_document->textDocument()->documentLayout()->disconnect(this);
    }
    m_document = document;
    if (QTextDocument *doc = document ? document->textDocument() : nullptr) {
        // Text set, wrapped differently (resized), or highlighted: positions move.
        connect(doc->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, &m_timer, qOverload<>(&QTimer::start));
    }
    Q_EMIT documentChanged();
    m_timer.start();
}

void LineNumbers::setTop(qreal top)
{
    if (!qFuzzyCompare(top + 1, m_top + 1)) {
        m_top = top;
        Q_EMIT viewChanged();
        m_timer.start();
    }
}

void LineNumbers::setHeight(qreal height)
{
    if (!qFuzzyCompare(height + 1, m_height + 1)) {
        m_height = height;
        Q_EMIT viewChanged();
        m_timer.start();
    }
}

QVariantList LineNumbers::lines() const
{
    return m_lines;
}

int LineNumbers::count() const
{
    return m_count;
}

void LineNumbers::update()
{
    QVariantList lines;
    QTextDocument *doc = m_document ? m_document->textDocument() : nullptr;
    const int count = doc ? doc->blockCount() : 0;
    if (doc && count > 0 && m_height > 0) {
        QAbstractTextDocumentLayout *layout = doc->documentLayout();
        const auto top = [&](int n) {
            return layout->blockBoundingRect(doc->findBlockByNumber(n)).top();
        };
        // The last line starting at or above the visible top (binary search).
        int lo = 0;
        int hi = count - 1;
        while (lo < hi) {
            const int mid = (lo + hi + 1) / 2;
            if (top(mid) <= m_top) {
                lo = mid;
            } else {
                hi = mid - 1;
            }
        }
        const qreal bottom = m_top + m_height;
        for (QTextBlock block = doc->findBlockByNumber(lo); block.isValid(); block = block.next()) {
            const qreal y = layout->blockBoundingRect(block).top();
            if (y > bottom) {
                break;
            }
            lines.append(QVariantMap{{u"number"_s, block.blockNumber() + 1}, {u"y"_s, y}});
        }
    }
    if (lines != m_lines || count != m_count) {
        m_lines = lines;
        m_count = count;
        Q_EMIT linesChanged();
    }
}
