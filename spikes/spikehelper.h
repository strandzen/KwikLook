// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QTimer>

class KWindowShadow;
#include <QWindow>

// Throwaway helper for the step 1 experiments (frame variants, Wayland focus).
// Not part of KwikLook; see spikes/README.md.
class SpikeHelper : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.strandzen.KwikLook.Spike")
    QML_ELEMENT
    QML_SINGLETON

    // What this process shows: "chooser", "native", "notitle", "frameless", "trigger", "viewer".
    Q_PROPERTY(QString mode READ mode CONSTANT)
    Q_PROPERTY(QString sampleImage READ sampleImage CONSTANT)
    Q_PROPERTY(QString backdropImage READ backdropImage CONSTANT)
    Q_PROPERTY(bool viewerReady READ viewerReady NOTIFY viewerReadyChanged)

public:
    explicit SpikeHelper(QObject *parent = nullptr);
    static SpikeHelper *create(QQmlEngine *, QJSEngine *);
    static void setMode(const QString &mode);

    QString mode() const;
    QString sampleImage() const;
    QString backdropImage() const;
    bool viewerReady() const;

    // Frame test: starts this program again showing one variant.
    Q_INVOKABLE void openVariant(const QString &variant);
    // Frame test, variant "frameless": a compositor-drawn shadow imitating Breeze.
    Q_INVOKABLE void applyShadow(QWindow *window, int cornerRadius);

    // Focus test, trigger side: register on D-Bus and start the viewer process.
    Q_INVOKABLE void registerTrigger();
    // Focus test, trigger side: ask the viewer process to show its window.
    Q_INVOKABLE void showViewer(QWindow *trigger, bool withToken);
    // Focus test, viewer side: register on D-Bus and remember the window.
    Q_INVOKABLE void registerViewer(QWindow *viewer);

    // Focus test, automatic run for the nested session: show with and without token.
    Q_INVOKABLE void runAutomatic(QWindow *trigger);

public Q_SLOTS:
    // D-Bus (viewer side).
    Q_SCRIPTABLE void Show(const QString &token);
    Q_SCRIPTABLE void Hide();
    // D-Bus (trigger side): the viewer reports what happened.
    Q_SCRIPTABLE void Report(const QString &message);

Q_SIGNALS:
    void log(const QString &message);
    void viewerReadyChanged();

private:
    void startViewer();

    QPointer<QWindow> m_viewer;
    QList<KWindowShadow *> m_shadows;
    bool m_viewerReady = false;
    int m_showCount = 0;
};
