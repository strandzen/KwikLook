// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "windowframe.h"

#include "kwiklooksettings.h"
#include "shadowimage.h"

#include <KDirWatch>
#include <KWindowShadow>

#include <QLoggingCategory>
#include <QPlatformSurfaceEvent>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcFrame, "kwiklook.frame", QtInfoMsg)

WindowFrame::WindowFrame(QObject *parent)
    : QObject(parent)
    , m_style(BreezeStyle::load(breezercPath()))
{
    // Breeze's settings page saves breezerc; follow it.
    KDirWatch::self()->addFile(breezercPath());
    connect(KDirWatch::self(), &KDirWatch::dirty, this, &WindowFrame::reloadStyle);
    connect(KDirWatch::self(), &KDirWatch::created, this, &WindowFrame::reloadStyle);
    connect(KDirWatch::self(), &KDirWatch::deleted, this, &WindowFrame::reloadStyle);
    // KwikLook's own corner radius setting.
    const auto radiusChanged = [this] {
        Q_EMIT styleChanged();
        if (m_window && m_window->handle()) {
            updateShadow();
        }
    };
    connect(KwikLookSettings::self(), &KwikLookSettings::MatchBreezeCornersChanged, this, radiusChanged);
    connect(KwikLookSettings::self(), &KwikLookSettings::CornerRadiusChanged, this, radiusChanged);
}

WindowFrame::~WindowFrame()
{
    destroyShadow();
}

QString WindowFrame::breezercPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u"/breezerc"_s;
}

QWindow *WindowFrame::window() const
{
    return m_window;
}

void WindowFrame::setWindow(QWindow *window)
{
    if (window == m_window) {
        return;
    }
    destroyShadow();
    if (m_window) {
        m_window->removeEventFilter(this);
    }
    m_window = window;
    if (m_window) {
        m_window->installEventFilter(this);
        if (m_window->handle()) {
            updateShadow();
        }
    }
    Q_EMIT windowChanged();
}

int WindowFrame::radius() const
{
    const KwikLookSettings *settings = KwikLookSettings::self();
    return settings->matchBreezeCorners() ? m_style.cornerRadius() : settings->cornerRadius();
}

bool WindowFrame::outline() const
{
    return m_style.outline;
}

bool WindowFrame::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window && event->type() == QEvent::PlatformSurface) {
        // A shadow belongs to one native surface; hiding a window on Wayland
        // destroys it, showing creates a new one.
        switch (static_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType()) {
        case QPlatformSurfaceEvent::SurfaceCreated:
            updateShadow();
            break;
        case QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed:
            destroyShadow();
            break;
        }
    }
    return QObject::eventFilter(watched, event);
}

void WindowFrame::reloadStyle(const QString &path)
{
    Q_UNUSED(path)
    const BreezeStyle style = BreezeStyle::load(breezercPath());
    if (style == m_style) {
        return;
    }
    m_style = style;
    Q_EMIT styleChanged();
    if (m_window && m_window->handle()) {
        updateShadow();
    }
}

void WindowFrame::updateShadow()
{
    destroyShadow();
    if (!m_window) {
        return;
    }
    const ShadowImage shadow = renderShadow(m_style.shadowLayers(), radius(), m_style.shadowColor);
    if (shadow.image.isNull()) {
        return;
    }
    const auto tile = [&shadow](const QRect &r) {
        auto t = KWindowShadowTile::Ptr::create();
        t->setImage(shadow.image.copy(r));
        return t;
    };
    m_shadow = new KWindowShadow(this);
    m_shadow->setTopLeftTile(tile(shadow.topLeft));
    m_shadow->setTopTile(tile(shadow.top));
    m_shadow->setTopRightTile(tile(shadow.topRight));
    m_shadow->setRightTile(tile(shadow.right));
    m_shadow->setBottomRightTile(tile(shadow.bottomRight));
    m_shadow->setBottomTile(tile(shadow.bottom));
    m_shadow->setBottomLeftTile(tile(shadow.bottomLeft));
    m_shadow->setLeftTile(tile(shadow.left));
    m_shadow->setPadding(shadow.padding);
    m_shadow->setWindow(m_window);
    if (!m_shadow->create()) {
        qCWarning(lcFrame) << "the compositor did not accept the window shadow";
    }
}

void WindowFrame::destroyShadow()
{
    if (m_shadow) {
        m_shadow->destroy();
        delete m_shadow;
        m_shadow = nullptr;
    }
}
