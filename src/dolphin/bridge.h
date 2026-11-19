// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 KDEGlance contributors
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
// Based on KDEGlance's Dolphin hook (https://github.com/AlexGraveler/KDEGlance).
#pragma once

#include <KConfigWatcher>
#include <KFileItem>
#include <KSharedConfig>

#include <QElapsedTimer>
#include <QHash>
#include <QKeyCombination>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QUrl>

class QKeyEvent;
class QWidget;

/*
 * Lives inside the Dolphin process (one per process, owned by qApp) and
 * connects Dolphin with the KwikLook viewer.
 *
 *  - Space: an application-wide event filter sees the key before Dolphin's
 *    own shortcuts. When the keyboard focus is in a file view and Dolphin is
 *    not in type-ahead search or selection mode, Space opens or closes the
 *    preview; everywhere else it is left alone. Folders keep Dolphin's own
 *    Space behaviour.
 *  - Order: the files of the view in Dolphin's display order go with the
 *    request, so ← → in the preview follow Dolphin.
 *  - Selection sync: the viewer's CurrentChanged/Closed signals select the
 *    previewed file in the view that opened the preview, and only there. The
 *    bridge ignores the selection changes it causes itself (no loops). If
 *    Dolphin keeps the keyboard focus, the preview follows its selection.
 *  - Fallback: if a Dolphin update removed the internals DolphinAccess needs,
 *    Space previews the selected file alone (Dolphin's public selection signal).
 *  - Starts the viewer (D-Bus activation) when Dolphin starts, so the first
 *    preview opens at once.
 *
 * All D-Bus calls from here are asynchronous: Dolphin never waits for the viewer.
 */
class Bridge : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.strandzen.KwikLook.DolphinBridge")
public:
    // Creates the bridge once, if running inside Dolphin.
    static Bridge *ensure();
    ~Bridge() override;

public Q_SLOTS:
    // Diagnostics and tests (Dolphin's bus name, /KwikLookBridge): a JSON summary,
    // and what Space would do in the focused file view.
    Q_SCRIPTABLE QString State();
    Q_SCRIPTABLE bool TriggerFocusedView();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private Q_SLOTS:
    void onViewerCurrentChanged(const QString &session, const QString &url, int index);
    void onViewerClosed(const QString &session);
    void onFallbackSelectionChanged(const KFileItemList &selection);
    void onSessionSelectionChanged(const KFileItemList &selection);

private:
    void readOpenKey();
    explicit Bridge(QObject *parent);

    bool canHandleKeyIn(QWidget *view) const;
    bool trigger(QWidget *view);
    void open(int index, const QString &activationToken);
    void callViewer(const QString &method, const QVariantList &args);
    void endSession();
    void syncSelection(const QString &url);
    void trackFallbackSelection(QWidget *view);
    QWidget *activeFileView() const;
    void shutdown();

    QPointer<QWidget> m_sessionView;
    QString m_sessionId; // empty: no preview from this process
    quint64 m_sessionCounter = 0;
    QString m_tokenSession; // session waiting for its activation token (Wayland)
    QHash<QString, QUrl> m_localToModelUrl; // the viewer gets local URLs, Dolphin's model may not
    QStringList m_sessionUrls; // list sent to the viewer
    QString m_viewerCurrent; // file the viewer shows
    bool m_applyingSync = false; // selection changes caused by us

    QElapsedTimer m_lastTypeAheadKey;
    QHash<QWidget *, KFileItemList> m_fallbackSelection;
    QSet<QWidget *> m_trackedViews;
    bool m_down = false;
    KSharedConfig::Ptr m_config; // kwiklookrc
    KConfigWatcher::Ptr m_configWatcher; // the settings window changed the open key
    QKeyCombination m_openKey;
};
