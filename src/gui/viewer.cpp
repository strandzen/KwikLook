// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "viewer.h"

#include "kwiklooksettings.h"

#include <KConfigGroup>
#include <KFileUtils>
#include <KFormat>
#include <KIO/FileCopyJob>
#include <KSharedConfig>
#include <KLocalizedString>
#include <KWindowSystem>

#include <algorithm>
#include <cmath>

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QProcess>
#include <QStandardPaths>
#include <QRawFont>
#include <QQuickItem>
#include <QQuickWindow>
#include <QDBusConnection>
#include <QDir>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QTimer>
#include <QtConcurrentRun>
#include <QFuture>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcViewer, "kwiklook.viewer", QtInfoMsg)

namespace
{
Viewer *s_instance = nullptr;
constexpr int FocusLossDelayMs = 150;
// How long a preview may wait for its first file's details before it shows anyway.
constexpr int ShowDelayMs = 150;
// How long a Placement() call waits for the file's details at most.
constexpr int PlacementWaitMs = 1000;
}

namespace
{
KConfigGroup stateGroup()
{
    // Window state, not a setting: ~/.local/state/kwiklookstaterc.
    return KSharedConfig::openStateConfig()->group(u"Viewer"_s);
}

// Details read ahead still describe the file: same size and date (local files).
bool unchangedOnDisk(const FileInfo &info)
{
    const QFileInfo now(info.url.toLocalFile());
    return info.url.isLocalFile() && now.exists() == info.exists
        && (!info.exists || (now.lastModified() == info.modified && (info.isDir || now.size() == info.size)));
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
    , m_pagesOpen(stateGroup().readEntry("PageSidebarOpen", false))
    , m_sidebarTab(stateGroup().readEntry("SidebarTab", u"contents"_s))
    , m_mediaVolume(std::clamp(stateGroup().readEntry("MediaVolume", 1.0), 0.0, 1.0))
    , m_office(new OfficeConverter(this))
    , m_book(new BookReader(this))
    , m_table(new TableModel(this))
{
    connect(m_office, &OfficeConverter::converted, this, [this](const QString &path, const QUrl &pdf) {
        if (path != currentUrl().toLocalFile()) {
            return;
        }
        m_convertedPdf = pdf;
        m_previewNote.clear();
        m_previewBusy = false;
        m_view = u"PdfView"_s;
        Q_EMIT fileInfoChanged();
    });
    connect(m_book, &BookReader::finished, this, [this](const QString &path, const Epub::Book &book) {
        if (path != currentUrl().toLocalFile()) {
            return;
        }
        m_info.text = book.html;
        m_info.textTruncated = book.truncated;
        m_info.formattedKind = u"book"_s;
        m_bookContents = book.contents;
        if (!book.title.isEmpty()) {
            m_info.details.append({i18nc("@label of a book", "Title"), book.title});
        }
        if (!book.author.isEmpty()) {
            m_info.details.append({i18nc("@label of a book", "Author"), book.author});
        }
        m_previewNote.clear();
        m_previewBusy = false;
        m_view = u"TextView"_s;
        Q_EMIT fileInfoChanged();
    });
    connect(m_book, &BookReader::failed, this, [this](const QString &path, BookReader::Failure failure) {
        if (path != currentUrl().toLocalFile()) {
            return;
        }
        m_previewBusy = false;
        m_previewNote = failure == BookReader::Failure::NoSandbox
            ? i18n("This system can't open books shut away from your files and the network, so books aren't previewed.")
            : i18n("This book couldn't be read.");
        Q_EMIT fileInfoChanged();
    });
    connect(m_office, &OfficeConverter::failed, this, [this](const QString &path, OfficeConverter::Failure failure) {
        if (path != currentUrl().toLocalFile()) {
            return;
        }
        m_previewBusy = false;
        m_previewNote = failure == OfficeConverter::Failure::NoSandbox
            ? i18n("This system can't run LibreOffice shut away from your files and the network, so office documents aren't previewed.")
            : i18n("LibreOffice couldn't convert this document.");
        Q_EMIT fileInfoChanged();
    });
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
                // Closed while a video was fullscreen: open as a normal window next time.
                m_window->setWindowStates(Qt::WindowNoState);
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

namespace
{
bool pdfPagesRendering(const QQuickItem *item)
{
    if (item->inherits("QQuickPdfPageImage") && item->property("status").toInt() == 2 /* Image.Loading */) {
        return true;
    }
    const auto children = item->childItems();
    return std::any_of(children.begin(), children.end(), pdfPagesRendering);
}
} // namespace

bool Viewer::finishRendering(int timeoutMs)
{
    auto *window = qobject_cast<QQuickWindow *>(m_window.data());
    if (!window) {
        return true;
    }
    window->hide();
    const QDeadlineTimer deadline(timeoutMs);
    // Quiet for a while, not just once: a layout queued with Qt.callLater (after a
    // zoom, say) can start another page a moment after a first check came out clear.
    QElapsedTimer quiet;
    quiet.start();
    while (quiet.elapsed() < 100) {
        if (pdfPagesRendering(window->contentItem())) {
            quiet.restart();
        }
        if (deadline.hasExpired()) {
            qCWarning(lcViewer) << "quitting while a PDF page still renders";
            return false;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return true;
}

void Viewer::closeIfFocusLeft()
{
    if (!QGuiApplication::focusWindow() && m_open && KwikLookSettings::self()->closeOnFocusLoss()) {
        Close();
    }
}

QUrl Viewer::currentUrl() const
{
    return m_shownUrl;
}

QString Viewer::fileName() const
{
    return currentUrl().fileName();
}

int Viewer::currentIndex() const
{
    return m_shownIndex;
}

bool Viewer::switching() const
{
    return m_index != m_shownIndex || m_urls.value(m_index) != m_shownUrl;
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
    if (m_info.formattedKind == u"book"_s) {
        return i18n("This book is long: showing only its beginning.");
    }
    return i18n("This file is long: showing only its beginning.");
}

QUrl Viewer::pdfUrl() const
{
    if (!m_convertedPdf.isEmpty()) {
        return m_convertedPdf;
    }
    return m_info.mimeType == u"application/pdf"_s ? currentUrl() : QUrl();
}

QString Viewer::previewNote() const
{
    return m_previewNote;
}

bool Viewer::previewBusy() const
{
    return m_previewBusy;
}

bool Viewer::officePreviewsAvailable() const
{
    return OfficeConverter::available();
}

void Viewer::startBookPreview()
{
    if (m_info.mimeType != u"application/epub+zip"_s || !m_info.exists || !currentUrl().isLocalFile()) {
        return;
    }
    m_previewBusy = true;
    m_previewNote = i18n("Preparing preview…");
    m_view = u"FallbackView"_s;
    m_book->read(currentUrl().toLocalFile());
}

void Viewer::startOfficePreview()
{
    if (!isOfficeDocument(m_info.mimeType) || !m_info.exists || !currentUrl().isLocalFile() || !KwikLookSettings::self()->officePreviews()
        || !OfficeConverter::available()) {
        return;
    }
    m_previewBusy = true;
    m_previewNote = i18n("Preparing preview…");
    m_view = u"FallbackView"_s; // also for flat XML documents, which would show as text
    m_office->convert(currentUrl().toLocalFile()); // may answer at once (cached)
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

qreal Viewer::mediaVolume() const
{
    return m_mediaVolume;
}

void Viewer::setMediaVolume(qreal volume)
{
    volume = std::clamp(volume, 0.0, 1.0);
    if (qFuzzyCompare(volume, m_mediaVolume)) {
        return;
    }
    m_mediaVolume = volume;
    // Written now, saved to disk when the preview closes (a slider moves in many steps).
    stateGroup().writeEntry("MediaVolume", volume);
    Q_EMIT mediaVolumeChanged();
}

void Viewer::loadFont()
{
    unloadFont();
    if (m_view != u"FontView"_s) {
        return;
    }
    // FreeType parses the font here, on the main thread: a few milliseconds.
    const QRawFont raw(m_info.fontData, 12);
    // Installed already? Asked before the preview adds its own copy.
    m_fontInstalled = raw.isValid() && QFontDatabase::families().contains(raw.familyName())
        && QFontDatabase::styles(raw.familyName()).contains(raw.styleName(), Qt::CaseInsensitive);
    m_fontId = QFontDatabase::addApplicationFontFromData(m_info.fontData);
    m_info.fontData.clear(); // Qt keeps its own copy
    if (m_fontId < 0 || !raw.isValid()) {
        qCWarning(lcViewer) << "could not load the font" << currentUrl();
        unloadFont();
        m_view = u"FallbackView"_s;
        return;
    }
    m_fontFamily = raw.familyName();
    m_fontStyle = raw.styleName();
}

void Viewer::unloadFont()
{
    if (m_fontId >= 0) {
        QFontDatabase::removeApplicationFont(m_fontId);
        m_fontId = -1;
    }
    m_fontFamily.clear();
    m_fontStyle.clear();
    m_fontInstalled = false;
}

bool Viewer::fontInstalled() const
{
    return m_fontInstalled;
}

void Viewer::installFont()
{
    const QUrl source = currentUrl();
    if (m_fontFamily.isEmpty() || m_fontInstalled || !source.isLocalFile()) {
        return;
    }
    // Where KDE's font installer puts fonts for one user; fontconfig reads it.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + u"/fonts"_s;
    if (!QDir().mkpath(dir)) {
        Q_EMIT fontInstallFinished(false);
        return;
    }
    QString name = source.fileName();
    if (QFileInfo::exists(dir + u'/' + name)) {
        name = KFileUtils::suggestName(QUrl::fromLocalFile(dir), name); // never overwrite
    }
    auto *job = KIO::file_copy(source, QUrl::fromLocalFile(dir + u'/' + name), -1, KIO::HideProgressInfo);
    connect(job, &KJob::result, this, [this, job, source, dir] {
        const bool ok = !job->error();
        if (!ok) {
            qCWarning(lcViewer) << "could not install the font:" << job->errorString();
        } else {
            // Other applications see it once fontconfig has read the folder again.
            if (!QStandardPaths::isTestModeEnabled()) { // tests: not the user's font cache
                QProcess::startDetached(u"fc-cache"_s, {dir});
            }
            if (currentUrl() == source) {
                m_fontInstalled = true;
                Q_EMIT fileInfoChanged();
            }
        }
        Q_EMIT fontInstallFinished(ok);
    });
}

QString Viewer::fontFamily() const
{
    return m_fontFamily;
}

QString Viewer::fontStyle() const
{
    return m_fontStyle;
}

QString Viewer::formattedKind() const
{
    return m_info.formattedKind;
}

QVariantList Viewer::bookContents() const
{
    return m_bookContents;
}

QString Viewer::formattedText() const
{
    return m_info.formattedText;
}

QVariantMap Viewer::desktopEntry() const
{
    return m_info.desktopEntry;
}

QVariantList Viewer::cards() const
{
    return m_info.cards;
}

int Viewer::moreCards() const
{
    return m_info.moreCards;
}

TableModel *Viewer::table() const
{
    return m_table;
}

namespace
{
KConfigGroup formattedGroup()
{
    // One entry per kind ("markdown=false": Markdown as source).
    return KSharedConfig::openStateConfig()->group(u"FormattedView"_s);
}
} // namespace

bool Viewer::showFormatted() const
{
    return m_info.formattedKind.isEmpty() || m_info.formattedKind == u"book"_s // no source view
        || formattedGroup().readEntry(m_info.formattedKind, true);
}

void Viewer::setShowFormatted(bool formatted)
{
    if (m_info.formattedKind.isEmpty() || m_info.formattedKind == u"book"_s || formatted == showFormatted()) {
        return;
    }
    KConfigGroup group = formattedGroup();
    group.writeEntry(m_info.formattedKind, formatted);
    group.sync();
    m_view = viewFor(m_info, formatted); // SVG: picture or text
    Q_EMIT fileInfoChanged();
}

QVariantMap Viewer::fileAt(int index) const
{
    const QUrl url = m_urls.value(index);
    if (url.isEmpty()) {
        return {};
    }
    const QMimeType mime = QMimeDatabase().mimeTypeForFile(url.path(), QMimeDatabase::MatchExtension);
    return {
        {u"url"_s, url},
        {u"name"_s, url.fileName()},
        {u"mimeType"_s, mime.name()},
        {u"iconName"_s, mime.iconName()},
    };
}

void Viewer::showFile(int index)
{
    if (m_open && index >= 0 && index < m_urls.size()) {
        setCurrent(index);
        if (m_indexOpen) {
            m_indexOpen = false; // the sheet's choice shows
            Q_EMIT indexOpenChanged();
        }
    }
}

bool Viewer::pagesOpen() const
{
    return m_pagesOpen;
}

void Viewer::setPagesOpen(bool open)
{
    if (open == m_pagesOpen) {
        return;
    }
    m_pagesOpen = open;
    KConfigGroup group = stateGroup();
    group.writeEntry("PageSidebarOpen", open);
    group.sync();
    Q_EMIT pagesOpenChanged();
}

QString Viewer::sidebarTab() const
{
    return m_sidebarTab;
}

void Viewer::setSidebarTab(const QString &tab)
{
    if (tab == m_sidebarTab) {
        return;
    }
    m_sidebarTab = tab;
    KConfigGroup group = stateGroup();
    group.writeEntry("SidebarTab", tab);
    group.sync();
    Q_EMIT sidebarTabChanged();
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
    const bool hidden = !m_window || !m_window->isVisible();
    setCurrent(qBound(0, index, int(m_urls.size()) - 1));
    if (m_placementActive && hidden) {
        m_showPending = true;
        m_pendingToken = activationToken;
        QTimer::singleShot(ShowDelayMs, this, &Viewer::showPendingWindow);
    } else {
        showWindow(activationToken);
    }
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
    const int generation = ++m_generation;
    Q_EMIT CurrentChanged(m_sessionId, m_urls.value(m_index).toString(), m_index); // Dolphin follows at once
    if (!m_window || !m_window->isVisible()) {
        // Nothing on screen to keep: the previous file's details go first. QML must
        // never see the new URL with the old type (it asked the JPEG thumbnailer for a
        // PNG that way).
        showCurrent(FileInfo());
    }
    Q_EMIT switchingChanged();
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
        FileInfo info = watcher->result();
        auto *window = qobject_cast<QQuickWindow *>(m_window.data());
        if (!window || !window->isVisible()) {
            showCurrent(std::move(info));
            showPendingWindow();
            return;
        }
        // QML takes its picture of the old view in the next frame; the switch follows
        // that frame (or a moment later: a covered window may get no frames).
        Q_EMIT aboutToSwitch();
        auto shown = std::make_shared<bool>(false);
        auto apply = [this, generation, shown, info = std::move(info)]() mutable {
            if (!*shown && generation == m_generation) {
                *shown = true;
                showCurrent(std::move(info));
            }
        };
        auto connection = std::make_shared<QMetaObject::Connection>();
        *connection = connect(window, &QQuickWindow::frameSwapped, this, [apply, connection]() mutable {
            QObject::disconnect(*connection);
            apply();
        }, Qt::QueuedConnection);
        QTimer::singleShot(100, this, [apply, connection]() mutable {
            QObject::disconnect(*connection);
            apply();
        });
        window->update();
    });
    // Read ahead and unchanged since: no need to read it again.
    const QUrl url = m_urls.value(m_index);
    if (const auto ahead = m_ahead.constFind(url); ahead != m_ahead.cend() && unchangedOnDisk(*ahead)) {
        watcher->setFuture(QtFuture::makeReadyValueFuture(*ahead));
        return;
    }
    watcher->setFuture(QtConcurrent::run([url] {
        return FileInfo::read(url);
    }));
}

void Viewer::readAhead()
{
    QList<QUrl> neighbours;
    for (const int i : {m_index + 1, m_index - 1}) {
        const QUrl url = m_urls.value(i);
        if (url.isLocalFile() && url != m_urls.value(m_index)) {
            neighbours << url;
        }
    }
    const qsizetype before = m_ahead.size();
    m_ahead.removeIf([&neighbours](const auto &entry) {
        return !neighbours.contains(entry.key());
    });
    bool changed = m_ahead.size() != before;
    for (const QUrl &url : std::as_const(neighbours)) {
        if (m_ahead.contains(url) || m_aheadReading.contains(url)) {
            continue;
        }
        m_aheadReading.insert(url);
        auto *watcher = new QFutureWatcher<FileInfo>(this);
        connect(watcher, &QFutureWatcher<FileInfo>::finished, this, [this, watcher, url] {
            watcher->deleteLater();
            m_aheadReading.remove(url);
            // Still next to the current file? (← → may have moved on meanwhile.)
            if (m_open && (m_urls.value(m_index + 1) == url || m_urls.value(m_index - 1) == url)) {
                m_ahead.insert(url, watcher->result());
                Q_EMIT picturesAheadChanged();
            }
        });
        watcher->setFuture(QtConcurrent::run([url] {
            return FileInfo::read(url);
        }));
    }
    if (changed) {
        Q_EMIT picturesAheadChanged();
    }
}

QVariantList Viewer::picturesAhead() const
{
    QVariantList pictures;
    for (const int i : {m_index + 1, m_index - 1}) {
        const FileInfo info = m_ahead.value(m_urls.value(i));
        // Still pictures only, as ImageView loads them (SVG is drawn at the size shown).
        if (info.known && viewFor(info, false) == u"ImageView"_s && !info.animated && info.formattedKind.isEmpty()
            && info.imageSize.isValid()) {
            pictures << QVariantMap{{u"url"_s, info.url}, {u"width"_s, info.imageSize.width()}, {u"height"_s, info.imageSize.height()}};
        } else {
            pictures << QVariantMap();
        }
    }
    return pictures;
}

void Viewer::showCurrent(FileInfo info)
{
    m_shownUrl = m_urls.value(m_index);
    m_shownIndex = m_index;
    m_info = std::move(info);
    m_office->cancel();
    m_book->cancel();
    m_bookContents.clear();
    m_convertedPdf.clear();
    m_previewNote.clear();
    m_previewBusy = false;
    if (m_info.known) {
        m_table->setRows(std::exchange(m_info.tableRows, {})); // the model keeps them
        m_view = viewFor(m_info, showFormatted());
        loadFont();
        startOfficePreview(); // the fallback view meanwhile, then the PDF view
        startBookPreview();
    }
    // Everything is set before either goes out; views clear on the first, load on the second.
    Q_EMIT currentChanged();
    Q_EMIT fileInfoChanged();
    Q_EMIT switchingChanged();
    if (m_info.known) {
        answerPlacements();
        if (m_open) {
            readAhead();
        }
    }
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

void Viewer::showPendingWindow()
{
    if (m_showPending) {
        m_showPending = false;
        showWindow(std::exchange(m_pendingToken, QString()));
    }
}

void Viewer::setPlacementActive(bool active)
{
    m_placementActive = active;
}

QString Viewer::Placement(const QString &caption)
{
    if (calledFromDBus() && m_open && !m_info.known) {
        setDelayedReply(true);
        m_placementCalls.append(message());
        QTimer::singleShot(PlacementWaitMs, this, &Viewer::answerPlacements);
        return {};
    }
    return placementAnswer(caption);
}

void Viewer::answerPlacements()
{
    for (const QDBusMessage &call : std::exchange(m_placementCalls, {})) {
        QDBusConnection::sessionBus().send(call.createReply(placementAnswer(call.arguments().value(0).toString())));
    }
}

QString Viewer::placementAnswer(const QString &caption) const
{
    // The window manager's caption is our title, maybe with “— KwikLook” after it.
    const QString title = m_window ? m_window->title() : QString();
    const bool preview = m_open && !title.isEmpty() && (caption == title || caption.startsWith(title + u' '));
    QJsonObject answer{{u"preview"_s, preview}};
    if (!preview) {
        return QString::fromUtf8(QJsonDocument(answer).toJson(QJsonDocument::Compact));
    }
    QString mode = KwikLookSettings::self()->windowPosition().toLower();
    if (mode == u"dolphin"_s && m_sessionId.isEmpty()) {
        mode = u"screen"_s; // not opened from Dolphin (`kwiklook <files>`)
    }
    answer[u"mode"_s] = mode;
    const auto size = [](qreal width, qreal height) {
        return QJsonObject{{u"width"_s, std::ceil(width)}, {u"height"_s, std::ceil(height)}};
    };
    answer[u"picture"_s] = QJsonValue::Null;
    if (m_info.known && m_view == u"ImageView"_s && m_info.imageSize.isValid() && m_windowChrome.isValid()) {
        const qreal dpr = m_window->devicePixelRatio();
        answer[u"picture"_s] = size(m_info.imageSize.width() / dpr, m_info.imageSize.height() / dpr);
    }
    answer[u"chrome"_s] = size(m_windowChrome.width(), m_windowChrome.height());
    answer[u"minimum"_s] = size(m_window->minimumWidth(), m_window->minimumHeight());
    return QString::fromUtf8(QJsonDocument(answer).toJson(QJsonDocument::Compact));
}

void Viewer::markClosed()
{
    if (!m_open) {
        return;
    }
    m_open = false;
    m_showPending = false;
    m_ahead.clear(); // read again next time: the files may change meanwhile
    Q_EMIT picturesAheadChanged();
    m_office->cancel();
    m_book->cancel();
    unloadFont();
    if (!m_fontSampleText.isEmpty()) {
        m_fontSampleText.clear();
        Q_EMIT fontSampleTextChanged();
    }
    if (m_fontSampleSize != DefaultFontSampleSize) {
        m_fontSampleSize = DefaultFontSampleSize;
        Q_EMIT fontSampleSizeChanged();
    }
    if (m_searchOpen) {
        m_searchOpen = false;
        Q_EMIT searchOpenChanged();
    }
    if (m_indexOpen) {
        m_indexOpen = false;
        Q_EMIT indexOpenChanged();
    }
    if (!m_searchText.isEmpty()) {
        m_searchText.clear();
        Q_EMIT searchTextChanged();
    }
    if (m_textZoom != 1) {
        m_textZoom = 1;
        Q_EMIT textZoomChanged();
    }
    if (m_mediaRepeat) {
        m_mediaRepeat = false;
        Q_EMIT mediaRepeatChanged();
    }
    stateGroup().sync();
    Q_EMIT Closed(m_sessionId);
}
