// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "contents.h"

#include "headings.h"

#include <QAbstractItemModel>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QPdfBookmarkModel>
#include <QPdfDocument>
#include <QPointF>
#include <QQuickTextDocument>
#include <QTextBlock>
#include <QTextDocument>

using namespace Qt::StringLiterals;

namespace
{
void addBookmarks(const QAbstractItemModel *model, const QModelIndex &parent, int depth, const QHash<QByteArray, int> &roles, QVariantList &out)
{
    for (int row = 0; row < model->rowCount(parent); ++row) {
        const QModelIndex index = model->index(row, 0, parent);
        out.append(QVariantMap{
            {u"title"_s, index.data(roles.value("title")).toString().simplified()},
            {u"level"_s, depth + 1},
            {u"page"_s, index.data(roles.value("page")).toInt()},
            {u"y"_s, index.data(roles.value("location")).toPointF().y()},
        });
        addBookmarks(model, index, depth + 1, roles, out);
    }
}
}

QUrl PdfContents::source() const
{
    return m_source;
}

QVariantList PdfContents::entries() const
{
    return m_entries;
}

void PdfContents::setSource(const QUrl &source)
{
    if (source == m_source) {
        return;
    }
    m_source = source;
    Q_EMIT sourceChanged();
    const int generation = ++m_generation;
    if (!m_entries.isEmpty()) {
        m_entries.clear();
        Q_EMIT entriesChanged();
    }
    auto *watcher = new QFutureWatcher<QVariantList>(this);
    connect(watcher, &QFutureWatcher<QVariantList>::finished, this, [this, watcher, generation] {
        watcher->deleteLater();
        if (generation != m_generation) {
            return;
        }
        m_entries = watcher->result();
        if (!m_entries.isEmpty()) {
            Q_EMIT entriesChanged();
        }
    });
    watcher->setFuture(QtConcurrent::run(&PdfContents::read, source));
}

QVariantList PdfContents::read(const QUrl &url)
{
    QVariantList out;
    if (!url.isLocalFile()) {
        return out;
    }
    QPdfDocument document(nullptr);
    if (document.load(url.toLocalFile()) != QPdfDocument::Error::None) {
        return out; // damaged, locked: the view says so
    }
    QPdfBookmarkModel model(nullptr); // goes before the document
    model.setDocument(&document);
    QHash<QByteArray, int> roles; // by name, as QML knows them
    const auto names = model.roleNames();
    for (auto it = names.begin(); it != names.end(); ++it) {
        roles.insert(it.value(), it.key());
    }
    addBookmarks(&model, {}, 0, roles, out);
    return out;
}

QVariantList Contents::fromText(QQuickTextDocument *document, const QString &kind, bool rendered, const QString &source) const
{
    QVariantList out;
    const QTextDocument *doc = document ? document->textDocument() : nullptr;
    if (!doc || (kind != u"markdown" && kind != u"html")) {
        return out;
    }
    QList<Headings::Heading> headings;
    if (rendered) {
        headings = Headings::fromDocument(*doc);
    } else {
        // Rendered aside (not laid out, nothing loaded) for the titles, as shown.
        QTextDocument aside;
        if (kind == u"markdown") {
            aside.setMarkdown(source);
        } else {
            aside.setHtml(source);
        }
        headings = Headings::match(Headings::fromDocument(aside), Headings::fromSource(source, kind));
        for (Headings::Heading &heading : headings) {
            const QTextBlock line = doc->findBlockByNumber(heading.at); // a line of plain text is a block
            heading.at = line.isValid() ? line.position() : 0;
        }
    }
    for (const Headings::Heading &heading : std::as_const(headings)) {
        out.append(QVariantMap{{u"title"_s, heading.title}, {u"level"_s, heading.level}, {u"position"_s, heading.at}});
    }
    return out;
}
