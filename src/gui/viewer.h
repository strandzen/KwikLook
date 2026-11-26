// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include "dbusnames.h"
#include "fileinfo.h"
#include "officeconverter.h"
#include "tablemodel.h"

#include <QDBusContext>
#include <QDBusMessage>
#include <QFutureWatcher>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QtQml/qqmlregistration.h>
#include <QUrl>
#include <QWindow>

/*
 * The background viewer: the list of files being previewed, the current one,
 * and the preview window. Published on the session bus as
 * io.github.strandzen.KwikLook /Viewer; the Dolphin hook and `kwiklook <files>`
 * talk to it only through that interface.
 *
 * Session IDs are chosen by the caller (one per Dolphin window, say) and come
 * back in the signals, so each caller can ignore what another one started.
 */
class Viewer : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.strandzen.KwikLook.Viewer")
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QUrl currentUrl READ currentUrl NOTIFY currentChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY currentChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentChanged)
    Q_PROPERTY(int count READ count NOTIFY currentChanged)
    // While the window is shown, the current file (address, name, number and details)
    // stays until the next one's details are read, then all change in one step:
    // `switching` meanwhile (a busy indicator after a moment), aboutToSwitch() one frame
    // before (QML keeps a still picture of the old view to fade from).
    Q_PROPERTY(bool switching READ switching NOTIFY switchingChanged)
    // About the current file, filled in shortly after it becomes current (read off the
    // main thread); empty until then.
    Q_PROPERTY(QString view READ view NOTIFY fileInfoChanged) // QML file that shows it
    Q_PROPERTY(QString mimeType READ mimeType NOTIFY fileInfoChanged)
    Q_PROPERTY(QString iconName READ iconName NOTIFY fileInfoChanged)
    Q_PROPERTY(bool fileExists READ fileExists NOTIFY fileInfoChanged)
    Q_PROPERTY(QString details READ details NOTIFY fileInfoChanged) // type · size · date
    Q_PROPERTY(QSize imageSize READ imageSize NOTIFY fileInfoChanged) // pictures; invalid otherwise
    Q_PROPERTY(bool imageAnimated READ imageAnimated NOTIFY fileInfoChanged) // pictures with several frames
    Q_PROPERTY(QString text READ text NOTIFY fileInfoChanged) // text files: the start of the file
    // “This file is long: showing only its beginning.”, or empty.
    Q_PROPERTY(QString textNote READ textNote NOTIFY fileInfoChanged)
    // The PDF the PDF view shows: the file itself, or an office document converted
    // by LibreOffice; empty until there is one.
    Q_PROPERTY(QUrl pdfUrl READ pdfUrl NOTIFY fileInfoChanged)
    // About the preview itself, shown above the fallback view (“Preparing preview…”).
    Q_PROPERTY(QString previewNote READ previewNote NOTIFY fileInfoChanged)
    Q_PROPERTY(bool previewBusy READ previewBusy NOTIFY fileInfoChanged)
    // For the information panel: [{title, rows: [{label, value}]}], empty sections left out.
    Q_PROPERTY(QVariantList infoSections READ infoSections NOTIFY fileInfoChanged)
    // The information panel is open; remembered across previews and restarts.
    Q_PROPERTY(bool infoOpen READ infoOpen WRITE setInfoOpen NOTIFY infoOpenChanged)
    // The sidebar (PDF pages, contents of PDFs, Markdown and HTML) is open, and its tab
    // ("pages" or "contents"; a view shows the other when it hasn't that one); both
    // remembered across previews and restarts.
    Q_PROPERTY(bool pagesOpen READ pagesOpen WRITE setPagesOpen NOTIFY pagesOpenChanged)
    Q_PROPERTY(QString sidebarTab READ sidebarTab WRITE setSidebarTab NOTIFY sidebarTabChanged)
    // The index sheet (all files as a grid, to pick one) is shown; until the preview closes.
    Q_PROPERTY(bool indexOpen MEMBER m_indexOpen NOTIFY indexOpenChanged)
    // Text font size factor (Ctrl+wheel in text): kept from file to file until the preview closes.
    Q_PROPERTY(qreal textZoom MEMBER m_textZoom NOTIFY textZoomChanged)
    // Video and audio volume, 0-1 as the slider shows it (perceived loudness); remembered.
    Q_PROPERTY(qreal mediaVolume READ mediaVolume WRITE setMediaVolume NOTIFY mediaVolumeChanged)
    // Play videos and audio again and again: kept from file to file until the preview closes.
    Q_PROPERTY(bool mediaRepeat MEMBER m_mediaRepeat NOTIFY mediaRepeatChanged)
    // Font files: the loaded font's family and style name (for QML's font.family and
    // font.styleName); empty for other files.
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY fileInfoChanged)
    Q_PROPERTY(QString fontStyle READ fontStyle NOTIFY fileInfoChanged)
    // The same family and style is already among the system's or the user's fonts.
    Q_PROPERTY(bool fontInstalled READ fontInstalled NOTIFY fileInfoChanged)
    // The font preview's own text, typed by the user: kept until the preview closes.
    Q_PROPERTY(QString fontSampleText MEMBER m_fontSampleText NOTIFY fontSampleTextChanged)
    // Its size in pixels (− / +, Ctrl+wheel): also kept until the preview closes.
    Q_PROPERTY(int fontSampleSize MEMBER m_fontSampleSize NOTIFY fontSampleSizeChanged)
    // Find in text and PDFs (Ctrl+F): the bar is open, and what it looks for; kept from
    // file to file until the preview closes.
    Q_PROPERTY(bool searchOpen MEMBER m_searchOpen NOTIFY searchOpenChanged)
    Q_PROPERTY(QString searchText MEMBER m_searchText NOTIFY searchTextChanged)
    // Files that can show two ways (FileInfo::formattedKind: Markdown, HTML, SVG, JSON,
    // .desktop) show formatted, else as source with syntax colours; the bar's view
    // toggle switches. Remembered for each kind; every kind starts formatted.
    Q_PROPERTY(QString formattedKind READ formattedKind NOTIFY fileInfoChanged)
    Q_PROPERTY(bool showFormatted READ showFormatted WRITE setShowFormatted NOTIFY fileInfoChanged)
    Q_PROPERTY(QString formattedText READ formattedText NOTIFY fileInfoChanged) // JSON indented
    Q_PROPERTY(QVariantMap desktopEntry READ desktopEntry NOTIFY fileInfoChanged)
    // vCard / iCalendar: the contacts or events (FileInfo::cards), and how many more
    // the file has beyond those.
    Q_PROPERTY(QVariantList cards READ cards NOTIFY fileInfoChanged)
    Q_PROPERTY(int moreCards READ moreCards NOTIFY fileInfoChanged)
    // CSV / TSV: the file as a table (reset with each file).
    Q_PROPERTY(TableModel *table READ table CONSTANT)
    // The room around a picture shown at real size: the window's size minus the
    // picture's (bar, margins, information panel). Set by Main.qml; for Placement().
    Q_PROPERTY(QSize windowChrome MEMBER m_windowChrome NOTIFY windowChromeChanged)

public:
    static constexpr auto ServiceName = KwikLookDBus::Service;
    static constexpr auto ObjectPath = KwikLookDBus::ViewerPath;
    static constexpr auto InterfaceName = KwikLookDBus::ViewerInterface;
    static constexpr int DefaultFontSampleSize = 36;

    // No default argument on purpose: a default-constructible QML_SINGLETON is
    // built by the engine itself and create() is never called (see CLAUDE.md).
    explicit Viewer(QObject *parent);

    // The one viewer of the running app (also what QML gets).
    static Viewer *instance();
    static Viewer *create(QQmlEngine *, QJSEngine *);

    // Exports the object and takes the service name. False if another viewer has it.
    bool registerOnBus();
    // The preview window; shown on Open(), hidden on Close().
    void setWindow(QWindow *window);
    // Before the QML engine goes (quitting): hides the window and waits, at most
    // `timeoutMs`, until no PDF page is rendering. Qt PDF renders on the engine's image
    // thread and crashes when the document is deleted mid-render. False on timeout.
    bool finishRendering(int timeoutMs = 3000);
    // KwikLook's KWin script is loaded (src/gui/kwinscript.h): the window then waits,
    // briefly, for the first file's details before it shows, so the script can give it
    // the picture's shape as it appears.
    void setPlacementActive(bool active);

    QUrl currentUrl() const;
    QString fileName() const;
    int currentIndex() const;
    int count() const;
    bool switching() const;
    bool isOpen() const;
    QString view() const;
    QString mimeType() const;
    QString iconName() const;
    bool fileExists() const;
    QString details() const;
    QSize imageSize() const;
    bool imageAnimated() const;
    QString text() const;
    QString textNote() const;
    QUrl pdfUrl() const;
    QString previewNote() const;
    bool previewBusy() const;
    // LibreOffice and the sandbox helper are installed (for the settings window).
    Q_INVOKABLE bool officePreviewsAvailable() const;
    QVariantList infoSections() const;
    bool infoOpen() const;
    void setInfoOpen(bool open);
    // For the index sheet: the file at `index` as {url, name, mimeType, iconName}, its
    // type found from the name only (cheap, for the tiles on screen).
    Q_INVOKABLE QVariantMap fileAt(int index) const;
    // Shows the file at `index` (the index sheet's choice).
    Q_INVOKABLE void showFile(int index);
    bool pagesOpen() const;
    void setPagesOpen(bool open);
    QString sidebarTab() const;
    void setSidebarTab(const QString &tab);
    qreal mediaVolume() const;
    void setMediaVolume(qreal volume);
    QString fontFamily() const;
    QString fontStyle() const;
    bool fontInstalled() const;
    // Copies the current font file into the user's fonts (~/.local/share/fonts) in the
    // background; fontInstallFinished() tells how it went.
    Q_INVOKABLE void installFont();
    QString formattedKind() const;
    QString formattedText() const;
    QVariantMap desktopEntry() const;
    QVariantList cards() const;
    int moreCards() const;
    TableModel *table() const;
    bool showFormatted() const;
    void setShowFormatted(bool formatted);

public Q_SLOTS:
    // Shows `urls[index]` (local paths or URLs), with the others reachable by Next/Previous.
    // `activationToken` (Wayland) lets the preview take keyboard focus; may be empty.
    Q_SCRIPTABLE void Open(const QStringList &urls, int index, const QString &sessionId, const QString &activationToken);
    Q_SCRIPTABLE void Close();
    Q_SCRIPTABLE void Next();
    Q_SCRIPTABLE void Previous();
    // Ends the background viewer.
    Q_SCRIPTABLE void Quit();
    // Shows the settings window (`kwiklook --settings`, the “KwikLook Settings” launcher entry).
    Q_SCRIPTABLE void ShowSettings(const QString &activationToken);
    // For KwikLook's KWin script (data/kwin/kwiklook-placement.js), about the window with
    // this caption, as JSON: {preview, mode ("screen", "dolphin", "pointer"), picture
    // ({width, height} in logical pixels, or null: not a picture), chrome, minimum}.
    // `preview` is false for KwikLook's other windows (the settings window). Asked over
    // D-Bus while the file's details are still being read, it answers once they are in
    // (at most PlacementWaitMs later), so a picture's shape is known when the window opens.
    Q_SCRIPTABLE QString Placement(const QString &caption);

Q_SIGNALS:
    Q_SCRIPTABLE void CurrentChanged(const QString &sessionId, const QString &url, int index);
    Q_SCRIPTABLE void Closed(const QString &sessionId);
    void currentChanged();
    void fileInfoChanged();
    void switchingChanged();
    void aboutToSwitch();
    void infoOpenChanged();
    void pagesOpenChanged();
    void sidebarTabChanged();
    void indexOpenChanged();
    void textZoomChanged();
    void mediaVolumeChanged();
    void mediaRepeatChanged();
    void fontSampleTextChanged();
    void fontSampleSizeChanged();
    void fontInstallFinished(bool ok);
    void searchOpenChanged();
    void searchTextChanged();
    void windowChromeChanged();
    // main.cpp creates the settings window on the first request and shows it.
    void settingsRequested(const QString &activationToken);

private:
    void setCurrent(int index);
    void showWindow(const QString &activationToken);
    void showPendingWindow();
    QString placementAnswer(const QString &caption) const;
    void answerPlacements(); // the Placement() calls waiting for details
    void markClosed();
    void readFileInfo(int generation);
    // The file at m_index becomes the one shown, with these details.
    void showCurrent(FileInfo info);
    void closeIfFocusLeft();
    void startOfficePreview();
    // Font files: adds the current font to the app's fonts (the previous one goes).
    void loadFont();
    void unloadFont();

    QList<QUrl> m_urls;
    int m_index = -1; // the current file (Next / Previous go from here)
    QUrl m_shownUrl; // the file shown, see `switching`
    int m_shownIndex = -1;
    QString m_sessionId;
    bool m_open = false;
    QPointer<QWindow> m_window;
    FileInfo m_info;
    QString m_view; // kept while the next file's details load, so the view doesn't flicker
    bool m_infoOpen = false;
    bool m_pagesOpen = false;
    QString m_sidebarTab;
    bool m_indexOpen = false;
    qreal m_textZoom = 1;
    qreal m_mediaVolume = 1;
    bool m_mediaRepeat = false;
    int m_fontId = -1; // QFontDatabase application font of the current file
    QString m_fontFamily;
    QString m_fontStyle;
    bool m_fontInstalled = false;
    QString m_fontSampleText;
    int m_fontSampleSize = DefaultFontSampleSize;
    bool m_searchOpen = false;
    QString m_searchText;
    int m_generation = 0; // drops details read for a file that's no longer current
    OfficeConverter *m_office = nullptr;
    TableModel *m_table = nullptr;
    QUrl m_convertedPdf; // the current office document as PDF
    QString m_previewNote;
    bool m_previewBusy = false;
    QSize m_windowChrome;
    bool m_placementActive = false;
    bool m_showPending = false; // waiting for the first file's details, see setPlacementActive()
    QString m_pendingToken;
    QList<QDBusMessage> m_placementCalls; // waiting for the current file's details
};
