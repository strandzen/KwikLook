// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QAbstractTableModel>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

/*
 * A CSV / TSV file's rows for the table view (CsvView.qml): the first row is the
 * column headers, the rest the cells; rows are numbered from 1 below the headers.
 */
class TableModel : public QAbstractTableModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Viewer.table")

public:
    explicit TableModel(QObject *parent = nullptr);

    void setRows(const QList<QStringList> &rows);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // The longest text in a column (headers and the first rows), in characters: for
    // the column's width.
    Q_INVOKABLE int columnLength(int column) const;

private:
    QList<QStringList> m_rows; // m_rows[0]: headers
    int m_columns = 0;
};
