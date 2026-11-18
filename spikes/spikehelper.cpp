// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "spikehelper.h"

#include <KWaylandExtras>
#include <KWindowShadow>
#include <KWindowSystem>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QElapsedTimer>
#include <QFile>
#include <QFuture>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QWindow>

#include <cstdio>

using namespace Qt::StringLiterals;

namespace
{
QString s_mode = u"chooser"_s;
SpikeHelper *s_instance = nullptr;

const QString ViewerService = u"io.github.strandzen.KwikLook.Spike.Viewer"_s;
const QString TriggerService = u"io.github.strandzen.KwikLook.Spike.Trigger"_s;
const QString ObjectPath = u"/Spike"_s;
const QString Interface = u"io.github.strandzen.KwikLook.Spike"_s;
const QString ViewerAppId = u"io.github.strandzen.KwikLook.Spike.Viewer"_s;

// Three box blurs on the alpha channel approximate a Gaussian blur.
void boxBlurAlpha(QImage &img, int radius)
{
    if (radius < 1) {
        return;
    }
    const int w = img.width();
    const int h = img.height();
    QList<int> line(qMax(w, h));
    for (int pass = 0; pass < 3; ++pass) {
        for (int y = 0; y < h; ++y) { // horizontal
            uchar *row = img.scanLine(y);
            for (int x = 0; x < w; ++x) {
                line[x] = row[x];
            }
            int sum = 0;
            for (int x = -radius; x <= radius; ++x) {
                sum += line[qBound(0, x, w - 1)];
            }
            for (int x = 0; x < w; ++x) {
                row[x] = uchar(sum / (2 * radius + 1));
                sum += line[qMin(x + radius + 1, w - 1)] - line[qMax(x - radius, 0)];
            }
        }
        for (int x = 0; x < w; ++x) { // vertical
            for (int y = 0; y < h; ++y) {
                line[y] = img.scanLine(y)[x];
            }
            int sum = 0;
            for (int y = -radius; y <= radius; ++y) {
                sum += line[qBound(0, y, h - 1)];
            }
            for (int y = 0; y < h; ++y) {
                img.scanLine(y)[x] = uchar(sum / (2 * radius + 1));
                sum += line[qMin(y + radius + 1, h - 1)] - line[qMax(y - radius, 0)];
            }
        }
    }
}

struct Layer {
    int dy; // vertical offset of the shadow
    int blur; // box blur radius; three passes spread the shadow by 3 * blur
    qreal opacity;
};

KWindowShadowTile::Ptr tile(const QImage &img, const QRect &r)
{
    auto t = KWindowShadowTile::Ptr::create();
    t->setImage(img.copy(r));
    return t;
}
} // namespace

SpikeHelper::SpikeHelper(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
}

SpikeHelper *SpikeHelper::create(QQmlEngine *, QJSEngine *)
{
    auto *helper = s_instance ? s_instance : new SpikeHelper;
    QJSEngine::setObjectOwnership(helper, QJSEngine::CppOwnership);
    return helper;
}

void SpikeHelper::setMode(const QString &mode)
{
    s_mode = mode;
}

QString SpikeHelper::mode() const
{
    return s_mode;
}

QString SpikeHelper::sampleImage() const
{
    return u"file:///usr/share/wallpapers/Horos/contents/images/5120x2880.png"_s;
}

QString SpikeHelper::backdropImage() const
{
    return u"file:///usr/share/wallpapers/Next/contents/images/5120x2880.png"_s;
}

bool SpikeHelper::viewerReady() const
{
    return m_viewerReady;
}

void SpikeHelper::openVariant(const QString &variant)
{
    QProcess::startDetached(QCoreApplication::applicationFilePath(), {variant});
}

void SpikeHelper::applyShadow(QWindow *window, int cornerRadius)
{
    if (!window) {
        return;
    }
    // Breeze's default shadow, as two layers. Fitted to the shadow of a real Breeze
    // window measured in the nested session (within 0.01 on all four sides).
    const QList<Layer> layers = {{12, 21, 0.70}, {4, 8, 0.15}};

    int padTop = 0, padSide = 0, padBottom = 0, maxBlur = 0;
    for (const Layer &l : layers) {
        const int spread = l.blur * 3;
        padSide = qMax(padSide, spread);
        padTop = qMax(padTop, spread - l.dy);
        padBottom = qMax(padBottom, spread + l.dy);
        maxBlur = qMax(maxBlur, spread);
    }
    // The corner tiles reach into the window by `inner` so the blur around the
    // rounded corner is complete; the window's own shape is cut out below.
    const int inner = cornerRadius + maxBlur;
    const QSize box(2 * inner + 1, 2 * inner + 1);
    const QSize size(padSide + box.width() + padSide, padTop + box.height() + padBottom);
    const QRect windowRect(QPoint(padSide, padTop), box);

    QImage alpha(size, QImage::Format_Alpha8);
    alpha.fill(0);
    QImage shadow(size, QImage::Format_ARGB32_Premultiplied);
    shadow.fill(Qt::transparent);
    for (const Layer &l : layers) {
        alpha.fill(0);
        QPainter p(&alpha);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 255));
        p.drawRoundedRect(windowRect.translated(0, l.dy), cornerRadius, cornerRadius);
        p.end();
        boxBlurAlpha(alpha, l.blur);
        QImage layer(size, QImage::Format_ARGB32_Premultiplied);
        layer.fill(Qt::black);
        layer.setAlphaChannel(alpha);
        QPainter sp(&shadow);
        sp.setOpacity(l.opacity);
        sp.drawImage(0, 0, layer);
    }
    {
        QPainter p(&shadow); // no shadow under the window itself
        p.setRenderHint(QPainter::Antialiasing);
        p.setCompositionMode(QPainter::CompositionMode_Clear);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::black);
        p.drawRoundedRect(windowRect, cornerRadius, cornerRadius);
    }

    const int cl = padSide + inner; // corner tile width
    const int ct = padTop + inner; // top corner tile height
    const int cb = padBottom + inner; // bottom corner tile height
    const int midX = padSide + inner;
    const int midY = padTop + inner;
    const int w = size.width();
    const int h = size.height();

    auto *s = new KWindowShadow(this);
    s->setTopLeftTile(tile(shadow, QRect(0, 0, cl, ct)));
    s->setTopTile(tile(shadow, QRect(midX, 0, 1, padTop)));
    s->setTopRightTile(tile(shadow, QRect(w - cl, 0, cl, ct)));
    s->setRightTile(tile(shadow, QRect(w - padSide, midY, padSide, 1)));
    s->setBottomRightTile(tile(shadow, QRect(w - cl, h - cb, cl, cb)));
    s->setBottomTile(tile(shadow, QRect(midX, h - padBottom, 1, padBottom)));
    s->setBottomLeftTile(tile(shadow, QRect(0, h - cb, cl, cb)));
    s->setLeftTile(tile(shadow, QRect(0, midY, padSide, 1)));
    s->setPadding(QMargins(padSide, padTop, padSide, padBottom));
    s->setWindow(window);
    if (!s->create()) {
        qWarning("KWindowShadow::create() failed");
    }
    m_shadows << s;
}

void SpikeHelper::startViewer()
{
    auto *bus = QDBusConnection::sessionBus().interface();
    if (bus->isServiceRegistered(ViewerService)) {
        m_viewerReady = true;
        Q_EMIT viewerReadyChanged();
        return;
    }
    auto *watcher = new QDBusServiceWatcher(ViewerService, QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForRegistration, this);
    connect(watcher, &QDBusServiceWatcher::serviceRegistered, this, [this] {
        m_viewerReady = true;
        Q_EMIT viewerReadyChanged();
        Q_EMIT log(u"Viewer process is running in the background."_s);
    });
    // A child process stands in for KwikLook's background viewer; it ends with us.
    auto *viewer = new QProcess(this);
    viewer->setProcessChannelMode(QProcess::ForwardedChannels);
    viewer->start(QCoreApplication::applicationFilePath(), {u"viewer"_s});
}

void SpikeHelper::registerTrigger()
{
    QDBusConnection::sessionBus().registerObject(ObjectPath, this, QDBusConnection::ExportScriptableSlots);
    QDBusConnection::sessionBus().registerService(TriggerService);
    connect(this, &SpikeHelper::log, this, [](const QString &m) {
        std::printf("%s\n", qPrintable(m));
        std::fflush(stdout);
    });
    startViewer();
}

void SpikeHelper::showViewer(QWindow *trigger, bool withToken)
{
    const auto send = [this](const QString &token) {
        auto msg = QDBusMessage::createMethodCall(ViewerService, ObjectPath, Interface, u"Show"_s);
        msg << token;
        QDBusConnection::sessionBus().asyncCall(msg);
    };
    if (!withToken || !KWindowSystem::isPlatformWayland()) {
        Q_EMIT log(u"Asking the viewer to show itself, without a token…"_s);
        send(QString());
        return;
    }
    const quint32 serial = KWaylandExtras::lastInputSerial(trigger);
    Q_EMIT log(u"Requesting a focus token from KWin (input serial %1)…"_s.arg(serial));
    KWaylandExtras::xdgActivationToken(trigger, serial, ViewerAppId).then(this, [this, send](const QString &token) {
        Q_EMIT log(token.isEmpty() ? u"KWin gave no token."_s : u"Got a token, passing it to the viewer."_s);
        send(token);
    });
}

void SpikeHelper::registerViewer(QWindow *viewer)
{
    m_viewer = viewer;
    QDBusConnection::sessionBus().registerObject(ObjectPath, this, QDBusConnection::ExportScriptableSlots);
    QDBusConnection::sessionBus().registerService(ViewerService);
}

void SpikeHelper::runAutomatic(QWindow *trigger)
{
    // Without real input there is no input serial; this checks the plumbing,
    // the live test checks the real thing.
    QTimer::singleShot(2000, this, [this, trigger] {
        showViewer(trigger, true);
    });
    QTimer::singleShot(4000, this, [] {
        QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(ViewerService, ObjectPath, Interface, u"Hide"_s));
    });
    QTimer::singleShot(6000, this, [this, trigger] {
        showViewer(trigger, false);
    });
    QTimer::singleShot(8000, this, [] {
        QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(ViewerService, ObjectPath, Interface, u"Hide"_s));
    });
}

void SpikeHelper::Show(const QString &token)
{
    if (!m_viewer) {
        return;
    }
    const int n = ++m_showCount;
    const bool hadToken = !token.isEmpty();
    m_viewer->show();
    if (hadToken) {
        KWindowSystem::setCurrentXdgActivationToken(token);
    }
    KWindowSystem::activateWindow(m_viewer);

    auto *timer = new QElapsedTimer;
    timer->start();
    auto *done = new bool(false);
    const auto report = [n, hadToken, timer, done](bool focused) {
        if (*done) {
            return;
        }
        *done = true;
        const QString how = hadToken ? u"with token"_s : u"without token"_s;
        const QString msg = focused ? u"Preview %1 (%2): got keyboard focus after %3 ms."_s.arg(n).arg(how).arg(timer->elapsed())
                                    : u"Preview %1 (%2): did NOT get keyboard focus within 1.5 s."_s.arg(n).arg(how);
        auto call = QDBusMessage::createMethodCall(TriggerService, ObjectPath, Interface, u"Report"_s);
        call << msg;
        QDBusConnection::sessionBus().asyncCall(call);
    };
    if (m_viewer->isActive()) {
        report(true);
        return;
    }
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(m_viewer, &QWindow::activeChanged, this, [this, report, conn] {
        if (m_viewer->isActive()) {
            report(true);
            disconnect(*conn);
        }
    });
    QTimer::singleShot(1500, this, [report, conn] {
        report(false);
        QObject::disconnect(*conn);
    });
}

void SpikeHelper::Hide()
{
    if (m_viewer) {
        m_viewer->hide();
    }
}

void SpikeHelper::Report(const QString &message)
{
    Q_EMIT log(message);
}
