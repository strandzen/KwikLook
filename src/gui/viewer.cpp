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

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFontDatabase>
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
    , m_pagesOpen(stateGroup().readEntry("PageSidebarOpen", false))
    , m_mediaVolume(std::clamp(stateGroup().readEntry("MediaVolume", 1.0), 0.0, 1.0))
    , m_office(new OfficeConverter(this))
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
    return m_info.formattedKind.isEmpty() || formattedGroup().readEntry(m_info.formattedKind, true);
}

void Viewer::setShowFormatted(bool formatted)
{
    if (m_info.formattedKind.isEmpty() || formatted == showFormatted()) {
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
    m_office->cancel();
    m_convertedPdf.clear();
    m_previewNote.clear();
    m_previewBusy = false;
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
        m_table->setRows(std::exchange(m_info.tableRows, {})); // the model keeps them
        m_view = viewFor(m_info, showFormatted());
        loadFont();
        startOfficePreview(); // the fallback view meanwhile, then the PDF view
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
    m_office->cancel();
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
