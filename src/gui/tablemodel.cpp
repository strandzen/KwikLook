// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "tablemodel.h"

#include <algorithm>

TableModel::TableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void TableModel::setRows(const QList<QStringList> &rows)
{
    beginResetModel();
    m_rows = rows;
    m_columns = 0;
    for (const QStringList &row : std::as_const(m_rows)) {
        m_columns = std::max(m_columns, int(row.size()));
    }
    endResetModel();
}

int TableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() || m_rows.isEmpty() ? 0 : int(m_rows.size()) - 1;
}

int TableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_columns;
}

QVariant TableModel::data(const QModelIndex &index, int role) const
{
    if (role != Qt::DisplayRole || !index.isValid()) {
        return {};
    }
    return m_rows.value(index.row() + 1).value(index.column());
}

QVariant TableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) {
        return {};
    }
    if (orientation == Qt::Horizontal) {
        return m_rows.value(0).value(section);
    }
    return section + 1;
}

int TableModel::columnLength(int column) const
{
    int length = 0;
    // Enough rows to judge; the whole file could be thousands.
    const qsizetype rows = std::min<qsizetype>(m_rows.size(), 200);
    for (qsizetype r = 0; r < rows; ++r) {
        length = std::max(length, int(m_rows.at(r).value(column).size()));
    }
    return length;
}
