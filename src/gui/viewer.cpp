// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "viewer.h"

#include "kwiklooksettings.h"

#include <KConfigGroup>
#include <KFormat>
#include <KSharedConfig>
#include <KLocalizedString>
#include <KWindowSystem>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDir>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QTimer>
#include <QtConcurrentRun>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcViewer, "kwiklook.viewer", QtInfoMsg)

namespace
{
Viewer *s_instance = nullptr;
constexpr int FocusLossDelayMs = 150;
}

namespace
{
KConfigGroup stateGroup()
{
    // Window state, not a setting: ~/.local/state/kwiklookstaterc.
    return KSharedConfig::openStateConfig()->group(u"Viewer"_s);
}

QString formatDate(const QDateTime &date)
{
    return KFormat().formatRelativeDateTime(date, QLocale::ShortFormat); // “Today at 11:23”
}

// Like Dolphin's Properties dialog.
QString accessText(QFile::Permissions p, QFile::Permission read, QFile::Permission write)
{
    if ((p & read) && (p & write)) {
        return i18nc("@info file access", "Can view and change");
    }
    if (p & read) {
        return i18nc("@info file access", "Can only view");
    }
    if (p & write) {
        return i18nc("@info file access", "Can only change");
    }
    return i18nc("@info file access", "No access");
}

QVariantMap row(const QString &label, const QString &value)
{
    return {{u"label"_s, label}, {u"value"_s, value}};
}

QVariantMap section(const QString &title, const QVariantList &rows)
{
    return {{u"title"_s, title}, {u"rows"_s, rows}};
}
} // namespace

Viewer::Viewer(QObject *parent)
    : QObject(parent)
    , m_infoOpen(stateGroup().readEntry("InfoPanelOpen", false))
{
}

Viewer *Viewer::instance()
{
    if (!s_instance) {
        s_instance = new Viewer(QCoreApplication::instance());
    }
    return s_instance;
}

Viewer *Viewer::create(QQmlEngine *, QJSEngine *)
{
    Viewer *viewer = instance();
    // Owned by the application, not by the QML engine (see CLAUDE.md pitfalls).
    QJSEngine::setObjectOwnership(viewer, QJSEngine::CppOwnership);
    return viewer;
}

bool Viewer::registerOnBus()
{
    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerObject(QLatin1String(ObjectPath), this, QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals)) {
        qCWarning(lcViewer) << "could not export" << ObjectPath;
        return false;
    }
    if (!bus.registerService(QLatin1String(ServiceName))) {
        bus.unregisterObject(QLatin1String(ObjectPath));
        return false;
    }
    return true;
}

void Viewer::setWindow(QWindow *window)
{
    m_window = window;
    if (window) {
        // Closed by the window manager (Alt+F4) or hidden from QML: same as Close().
        connect(window, &QWindow::visibleChanged, this, [this](bool visible) {
            if (!visible) {
                markClosed();
            }
        });
        // Close when another application takes the focus. KwikLook's own windows (menus,
        // the “Open With” dialog, Share dialogs, the settings window) don't count: the
        // app then still has a focus window.
        connect(qGuiApp, &QGuiApplication::focusWindowChanged, this, [this](QWindow *focus) {
            if (!focus && m_open && KwikLookSettings::self()->closeOnFocusLoss()) {
                // Moving between our own windows can pass through "no focus" briefly.
                QTimer::singleShot(FocusLossDelayMs, this, &Viewer::closeIfFocusLeft);
            }
        });
    }
}

void Viewer::closeIfFocusLeft()
{
    if (!QGuiApplication::focusWindow() && m_open && KwikLookSettings::self()->closeOnFocusLoss()) {
        Close();
    }
}

QUrl Viewer::currentUrl() const
{
    return m_urls.value(m_index);
}

QString Viewer::fileName() const
{
    return currentUrl().fileName();
}

int Viewer::currentIndex() const
{
    return m_index;
}

int Viewer::count() const
{
    return m_urls.size();
}

bool Viewer::isOpen() const
{
    return m_open;
}

QString Viewer::view() const
{
    return m_view;
}

QString Viewer::mimeType() const
{
    return m_info.mimeType;
}

QString Viewer::iconName() const
{
    return m_info.iconName;
}

QSize Viewer::imageSize() const
{
    return m_info.imageSize;
}

bool Viewer::imageAnimated() const
{
    return m_info.animated;
}

QString Viewer::text() const
{
    return m_info.text;
}

QString Viewer::textNote() const
{
    if (!m_info.textTruncated) {
        return QString();
    }
    return i18n("This file is long: showing only its beginning.");
}

QVariantList Viewer::infoSections() const
{
    if (!m_info.known) {
        return {};
    }
    const FileInfo &i = m_info;
    QVariantList file{row(i18nc("@label", "Name"), fileName()), row(i18nc("@label", "Type"), i.typeName)};
    if (i.size >= 0) {
        file << row(i18nc("@label", "Size"),
                    i18nc("@info size, e.g. 8.3 MB (8,312,045 bytes)", "%1 (%2 bytes)", KFormat().formatByteSize(double(i.size)), QLocale().toString(i.size)));
    }
    if (i.url.isLocalFile()) {
        file << row(i18nc("@label folder the file is in", "Folder"), QDir::toNativeSeparators(QFileInfo(i.url.toLocalFile()).absolutePath()));
    } else {
        file << row(i18nc("@label", "Location"), i.url.adjusted(QUrl::RemoveFilename).toDisplayString());
    }
    if (i.modified.isValid()) {
        file << row(i18nc("@label", "Modified"), formatDate(i.modified));
    }
    if (i.created.isValid()) {
        file << row(i18nc("@label", "Created"), formatDate(i.created));
    }

    QVariantList details;
    if (i.imageSize.isValid()) {
        details << row(i18nc("@label picture size", "Dimensions"), i18nc("@info width × height", "%1 × %2 pixels", i.imageSize.width(), i.imageSize.height()));
    }
    for (const FileInfo::Detail &d : i.details) {
        details << row(d.label, d.value);
    }

    QVariantList permissions;
    if (i.exists && !i.owner.isEmpty()) {
        permissions << row(i18nc("@label", "Owner"), i.owner) << row(i18nc("@label", "Group"), i.group)
                    << row(i18nc("@label access of the owner", "Owner access"), accessText(i.permissions, QFile::ReadOwner, QFile::WriteOwner))
                    << row(i18nc("@label access of the group", "Group access"), accessText(i.permissions, QFile::ReadGroup, QFile::WriteGroup))
                    << row(i18nc("@label access of everyone else", "Others"), accessText(i.permissions, QFile::ReadOther, QFile::WriteOther));
    }

    QVariantList sections{section(i18nc("@title:group", "File"), file)};
    if (!details.isEmpty()) {
        sections << section(i18nc("@title:group", "Details"), details);
    }
    if (!permissions.isEmpty()) {
        sections << section(i18nc("@title:group", "Permissions"), permissions);
    }
    return sections;
}

bool Viewer::infoOpen() const
{
    return m_infoOpen;
}

void Viewer::setInfoOpen(bool open)
{
    if (open == m_infoOpen) {
        return;
    }
    m_infoOpen = open;
    KConfigGroup group = stateGroup();
    group.writeEntry("InfoPanelOpen", open);
    group.sync();
    Q_EMIT infoOpenChanged();
}

bool Viewer::fileExists() const
{
    return m_info.exists;
}

QString Viewer::details() const
{
    if (!m_info.known) {
        return QString();
    }
    if (!m_info.exists) {
        return i18n("This file no longer exists.");
    }
    QStringList parts{m_info.typeName};
    if (m_info.size >= 0) {
        parts << KFormat().formatByteSize(double(m_info.size));
    }
    if (m_info.modified.isValid()) {
        parts << i18nc("@info file modification time, e.g. “Modified yesterday at 14:03”", "Modified %1",
                       KFormat().formatRelativeDateTime(m_info.modified, QLocale::ShortFormat));
    }
    parts.removeAll(QString());
    return parts.join(u" · ");
}

void Viewer::Open(const QStringList &urls, int index, const QString &sessionId, const QString &activationToken)
{
    QList<QUrl> parsed;
    parsed.reserve(urls.size());
    for (const QString &u : urls) {
        const QUrl url = QUrl::fromUserInput(u, QString(), QUrl::AssumeLocalFile);
        if (url.isValid() && !url.isEmpty()) {
            parsed << url;
        }
    }
    if (parsed.isEmpty()) {
        qCInfo(lcViewer) << "Open() without usable URLs ignored";
        return;
    }
    // A different caller takes over: tell the previous one its preview is gone.
    if (m_open && sessionId != m_sessionId) {
        Q_EMIT Closed(m_sessionId);
    }
    m_urls = parsed;
    m_sessionId = sessionId;
    m_open = true;
    m_index = -1;
    setCurrent(qBound(0, index, int(m_urls.size()) - 1));
    showWindow(activationToken);
}

void Viewer::Close()
{
    if (m_window && m_window->isVisible()) {
        m_window->hide(); // visibleChanged → markClosed()
    } else {
        markClosed();
    }
}

void Viewer::Next()
{
    if (m_open && m_index + 1 < m_urls.size()) {
        setCurrent(m_index + 1);
    }
}

void Viewer::Previous()
{
    if (m_open && m_index > 0) {
        setCurrent(m_index - 1);
    }
}

void Viewer::Quit()
{
    Close();
    QCoreApplication::quit();
}

void Viewer::ShowSettings(const QString &activationToken)
{
    Q_EMIT settingsRequested(activationToken);
}

void Viewer::setCurrent(int index)
{
    if (index == m_index) {
        return;
    }
    m_index = index;
    // The previous file's details go first: QML must never see the new URL with
    // the old type (it asked the JPEG thumbnailer for a PNG that way).
    const int generation = ++m_generation;
    m_info = FileInfo();
    Q_EMIT fileInfoChanged();
    Q_EMIT currentChanged();
    Q_EMIT CurrentChanged(m_sessionId, currentUrl().toString(), m_index);
    readFileInfo(generation);
}

void Viewer::readFileInfo(int generation)
{
    auto *watcher = new QFutureWatcher<FileInfo>(this);
    connect(watcher, &QFutureWatcher<FileInfo>::finished, this, [this, watcher, generation] {
        watcher->deleteLater();
        if (generation != m_generation) {
            return; // another file became current meanwhile
        }
        m_info = watcher->result();
        m_view = viewFor(m_info);
        Q_EMIT fileInfoChanged();
    });
    watcher->setFuture(QtConcurrent::run([url = currentUrl()] {
        return FileInfo::read(url);
    }));
}

void Viewer::showWindow(const QString &activationToken)
{
    if (!m_window) {
        return;
    }
    m_window->show();
    if (!activationToken.isEmpty()) {
        KWindowSystem::setCurrentXdgActivationToken(activationToken);
    }
    KWindowSystem::activateWindow(m_window);
}

void Viewer::markClosed()
{
    if (!m_open) {
        return;
    }
    m_open = false;
    Q_EMIT Closed(m_sessionId);
}
