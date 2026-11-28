// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "archivemodel.h"

#include "sandbox.h"

#include <KFormat>
#include <KLocalizedString>

#include <QCollator>
#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QLocale>
#include <QLoggingCategory>
#include <QMimeDatabase>

#include <algorithm>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcArchive, "kwiklook.archive")

ArchiveModel::ArchiveModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    m_root.folder = true;
    m_run = new SandboxRun(TimeoutMs, this);
    connect(m_run, &SandboxRun::finished, this, [this](SandboxRun::End end, int exitCode, const QByteArray &output) {
        switch (end) {
        case SandboxRun::End::NoSandbox:
            m_status = u"unsupported"_s;
            Q_EMIT listingChanged();
            break;
        case SandboxRun::End::TimedOut:
            beginResetModel();
            m_root.children.clear();
            m_status = u"failed"_s;
            m_summary.clear();
            endResetModel();
            Q_EMIT listingChanged();
            break;
        case SandboxRun::End::Exited:
        case SandboxRun::End::Failed:
            finished(exitCode, output); // what could be read, or a note that nothing could
            break;
        }
    });
}

ArchiveModel::~ArchiveModel()
{
    m_run->cancel();
}

QString ArchiveModel::helper()
{
    const QString local = QCoreApplication::applicationDirPath() + u"/kwiklook-archive"_s;
    if (QFileInfo(local).isExecutable()) {
        return local;
    }
    const QString installed = QStringLiteral(KWIKLOOK_LIBEXECDIR "/kwiklook-archive");
    return QFileInfo(installed).isExecutable() ? installed : QString();
}

QUrl ArchiveModel::source() const
{
    return m_source;
}

void ArchiveModel::setSource(const QUrl &source)
{
    if (source == m_source) {
        return;
    }
    m_source = source;
    Q_EMIT sourceChanged();
    m_run->cancel();
    beginResetModel();
    m_root.children.clear();
    endResetModel();
    m_encrypted = false;
    m_more = 0;
    m_summary.clear();
    m_status.clear();
    const QString path = source.isLocalFile() ? source.toLocalFile() : QString();
    const QString lister = helper();
    if (path.isEmpty()) {
        Q_EMIT listingChanged();
        return;
    }
    if (Sandbox::binary().isEmpty() || lister.isEmpty() || !Sandbox::supported()) {
        m_status = u"unsupported"_s;
        Q_EMIT listingChanged();
        return;
    }
    m_status = u"reading"_s;
    Q_EMIT listingChanged();
    m_run->start({u"--read"_s, path, u"--read"_s, lister, u"--"_s, lister, path, QString::number(Limit), QString::number(Seconds)});
}

void ArchiveModel::finished(int exitCode, const QByteArray &output)
{
    const ArchiveListing::Listing listing = ArchiveListing::parse(output);
    if (listing.status.isEmpty()) {
        qCWarning(lcArchive) << "the lister ended without a result, exit code" << exitCode << m_source;
    } else if (!listing.message.isEmpty()) {
        qCInfo(lcArchive) << listing.message << m_source;
    }
    setListing(listing, QFileInfo(m_source.toLocalFile()).size());
}

void ArchiveModel::setListing(const ArchiveListing::Listing &listing, qint64 packedSize)
{
    beginResetModel();
    m_root.children.clear();
    int files = 0;
    int folders = 0;
    m_encrypted = false;
    // Children by name while building (a flat archive of 10,000 files: no scans).
    QHash<std::pair<const Node *, QString>, Node *> made;
    for (const ArchiveListing::Entry &entry : listing.entries) {
        const QStringList parts = ArchiveListing::parts(entry.path);
        if (parts.isEmpty()) {
            continue; // "./": the archive's own top
        }
        Node *node = &m_root;
        for (qsizetype i = 0; i < parts.size(); ++i) {
            const bool last = i == parts.size() - 1;
            const bool folder = !last || entry.kind == ArchiveListing::Kind::Folder;
            const QString key = (folder ? u"d"_s : u"f"_s) + parts.at(i);
            Node *child = made.value({node, key});
            if (!child) {
                auto created = std::make_unique<Node>();
                created->name = parts.at(i);
                created->folder = folder;
                created->parent = node;
                child = created.get();
                made.insert({node, key}, child);
                node->children.push_back(std::move(created));
                folder ? ++folders : ++files;
            }
            if (last) {
                child->link = entry.kind == ArchiveListing::Kind::Link;
                child->encrypted = entry.encrypted;
                child->size = folder || child->link ? -1 : entry.size; // a link's own size means nothing
                child->modified = entry.modified;
                m_encrypted = m_encrypted || entry.encrypted;
            }
            node = child;
        }
    }
    sortAndNumber(&m_root);
    m_more = std::max<qint64>(0, listing.total - listing.entries.size());

    const QString status = listing.status;
    m_status = status == u"ok"_s ? (m_more > 0 ? u"partial"_s : u"ready"_s)
        : status == u"locked"_s ? (m_root.children.empty() ? u"locked"_s : u"partial"_s)
        : status == u"time"_s || (status == u"error"_s && !m_root.children.empty()) ? u"partial"_s
        : u"failed"_s;
    if (status == u"ok"_s && listing.entries.isEmpty()) {
        m_status = u"ready"_s; // an empty archive
    }

    KFormat format;
    QStringList parts;
    if (m_more > 0) {
        parts << i18ncp("@info number of entries in an archive", "%1 item", "%1 items", listing.total);
    } else {
        parts << i18ncp("@info", "%1 file", "%1 files", files);
        if (folders > 0) {
            parts.last() += i18nc("@info list separator, then e.g. 14 folders", ", ") + i18ncp("@info", "%1 folder", "%1 folders", folders);
        }
    }
    if (listing.unpacked > 0 && status == u"ok"_s) {
        parts << i18nc("@info e.g. 45 MB unpacked (12 MB packed)", "%1 unpacked (%2 packed)", format.formatByteSize(double(listing.unpacked)),
                       format.formatByteSize(double(packedSize)));
    }
    m_summary = parts.join(u" · "_s);
    endResetModel();
    Q_EMIT listingChanged();
}

void ArchiveModel::sortAndNumber(Node *node)
{
    static QCollator collator = [] {
        QCollator c;
        c.setNumericMode(true);
        c.setCaseSensitivity(Qt::CaseInsensitive);
        return c;
    }();
    std::sort(node->children.begin(), node->children.end(), [](const auto &a, const auto &b) {
        if (a->folder != b->folder) {
            return a->folder;
        }
        return collator.compare(a->name, b->name) < 0;
    });
    for (std::size_t i = 0; i < node->children.size(); ++i) {
        node->children[i]->row = int(i);
        sortAndNumber(node->children[i].get());
    }
}

bool ArchiveModel::singleTopFolder() const
{
    return m_root.children.size() == 1 && m_root.children.front()->folder;
}

QString ArchiveModel::status() const
{
    return m_status;
}

QString ArchiveModel::summary() const
{
    return m_summary;
}

bool ArchiveModel::encrypted() const
{
    return m_encrypted;
}

qint64 ArchiveModel::more() const
{
    return m_more;
}

ArchiveModel::Node *ArchiveModel::nodeFor(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<Node *>(index.internalPointer()) : const_cast<Node *>(&m_root);
}

QModelIndex ArchiveModel::index(int row, int column, const QModelIndex &parent) const
{
    const Node *node = nodeFor(parent);
    if (row < 0 || std::size_t(row) >= node->children.size() || column < 0 || column >= 3) {
        return {};
    }
    return createIndex(row, column, node->children[row].get());
}

QModelIndex ArchiveModel::parent(const QModelIndex &child) const
{
    const Node *node = nodeFor(child);
    if (!child.isValid() || !node->parent || node->parent == &m_root) {
        return {};
    }
    return createIndex(node->parent->row, 0, node->parent);
}

int ArchiveModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) {
        return 0;
    }
    return int(nodeFor(parent)->children.size());
}

int ArchiveModel::columnCount(const QModelIndex &) const
{
    return 3;
}

QVariant ArchiveModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return {};
    }
    const Node *node = nodeFor(index);
    switch (role) {
    case Qt::DisplayRole:
        if (index.column() == 0) {
            return node->name;
        }
        if (index.column() == 1) {
            if (node->folder) {
                return i18ncp("@info number of items in a folder", "%1 item", "%1 items", int(node->children.size()));
            }
            return node->size >= 0 ? KFormat().formatByteSize(double(node->size)) : QString();
        }
        return node->modified >= 0 ? QLocale().toString(QDateTime::fromSecsSinceEpoch(node->modified), QLocale::ShortFormat) : QString();
    case IconNameRole:
        if (node->iconName.isEmpty()) {
            node->iconName = node->folder ? u"folder"_s
                : node->link             ? u"inode-symlink"_s
                                         : QMimeDatabase().mimeTypeForFile(node->name, QMimeDatabase::MatchExtension).iconName();
        }
        return node->iconName;
    case IsFolderRole:
        return node->folder;
    case EncryptedRole:
        return node->encrypted;
    }
    return {};
}

QVariant ArchiveModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    switch (section) {
    case 0: return i18nc("@title:column", "Name");
    case 1: return i18nc("@title:column", "Size");
    case 2: return i18nc("@title:column date and time", "Modified");
    }
    return {};
}

QHash<int, QByteArray> ArchiveModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();
    roles.insert(IconNameRole, "iconName");
    roles.insert(IsFolderRole, "isFolder");
    roles.insert(EncryptedRole, "encrypted");
    return roles;
}
