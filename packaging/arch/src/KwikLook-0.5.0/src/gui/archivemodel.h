// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include "archivelisting.h"

#include <QAbstractItemModel>
#include <QPointer>
#include <QProcess>
#include <QQmlEngine>
#include <QTimer>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <vector>

/*
 * What's inside an archive, as a folder tree for QML's TreeView: columns name, size,
 * modified; folders first. Listed by kwiklook-archive (libarchive) run in
 * kwiklook-sandbox, which may read the archive and nothing else.
 *
 *   ArchiveModel { id: archive; source: Viewer.currentUrl }
 *   TreeView { model: archive }   archive.status, archive.summary
 */
class ArchiveModel : public QAbstractItemModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    // "" (no source), "reading", "ready", "partial" (stopped or damaged: what was read),
    // "locked" (names need a password), "failed", "unsupported" (no sandbox here).
    Q_PROPERTY(QString status READ status NOTIFY listingChanged)
    // “128 files, 14 folders · 45 MB unpacked (12 MB packed)”.
    Q_PROPERTY(QString summary READ summary NOTIFY listingChanged)
    // Some entries are locked with a password.
    Q_PROPERTY(bool encrypted READ encrypted NOTIFY listingChanged)
    // Entries the archive has beyond those listed (the helper's limit).
    Q_PROPERTY(qint64 more READ more NOTIFY listingChanged)

public:
    enum Role { IconNameRole = Qt::UserRole + 1, IsFolderRole, EncryptedRole };
    static constexpr int Limit = 10000;
    static constexpr int Seconds = 5; // the helper stops counting then
    static constexpr int TimeoutMs = 15000; // and is stopped after this

    explicit ArchiveModel(QObject *parent = nullptr);
    ~ArchiveModel() override;

    QUrl source() const;
    void setSource(const QUrl &source);
    QString status() const;
    QString summary() const;
    bool encrypted() const;
    qint64 more() const;

    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Where the helper is: next to the binary in the build tree, else installed.
    static QString helper();
    // The archive has one folder at its top (and nothing else): the view opens it.
    Q_INVOKABLE bool singleTopFolder() const;
    // Builds the tree from a listing (also for tests, without the helper).
    void setListing(const ArchiveListing::Listing &listing, qint64 packedSize);

Q_SIGNALS:
    void sourceChanged();
    void listingChanged();

private:
    struct Node {
        QString name;
        bool folder = false;
        bool link = false;
        bool encrypted = false;
        qint64 size = -1;
        qint64 modified = -1;
        Node *parent = nullptr;
        int row = 0;
        std::vector<std::unique_ptr<Node>> children;
        mutable QString iconName; // looked up when first shown
    };

    void stop();
    void finished(int exitCode);
    Node *nodeFor(const QModelIndex &index) const;
    static void sortAndNumber(Node *node);

    QUrl m_source;
    QString m_status;
    QString m_summary;
    bool m_encrypted = false;
    qint64 m_more = 0;
    Node m_root;
    QPointer<QProcess> m_process;
    QTimer m_timeout;
};
