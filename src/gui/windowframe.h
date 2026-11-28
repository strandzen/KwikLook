// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include "breezestyle.h"

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QWindow>
#include <QtQml/qqmlregistration.h>

class KWindowShadow;

/*
 * Makes a frameless window look like a Breeze window: a compositor-drawn shadow
 * following the user's Breeze settings, plus the corner radius and outline flag
 * for QML to draw. Re-reads breezerc when it changes, and puts the shadow back
 * whenever the window gets a new surface (on Wayland every show after a hide).
 *
 *   WindowFrame { id: frame; window: root }
 */
class WindowFrame : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QWindow *window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(int radius READ radius NOTIFY styleChanged)
    Q_PROPERTY(bool outline READ outline NOTIFY styleChanged)

public:
    explicit WindowFrame(QObject *parent = nullptr);
    ~WindowFrame() override;

    QWindow *window() const;
    void setWindow(QWindow *window);
    int radius() const;
    bool outline() const;

    // Where Breeze keeps its settings (absolute path; may not exist).
    static QString breezercPath();

Q_SIGNALS:
    void windowChanged();
    void styleChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void reloadStyle(const QString &path = QString());
    void updateShadow();
    void destroyShadow();

    QPointer<QWindow> m_window;
    BreezeStyle m_style;
    KWindowShadow *m_shadow = nullptr;
};
