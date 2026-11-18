// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
//
// kwiklook-spike: throwaway experiments for milestone 1, step 1. See README.md.
//   kwiklook-spike [frame]          chooser for the three window frame variants
//   kwiklook-spike focus            Wayland focus test (stand-in for Dolphin + viewer)
// Internal: native | notitle | frameless | viewer; --backdrop, --open-all, --auto for the nested session.
#include "spikehelper.h"

#include <QApplication>
#include <QQmlApplicationEngine>


using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QStringList args = app.arguments().mid(1);
    const bool backdrop = args.removeAll(u"--backdrop"_s) > 0;
    const bool openAll = args.removeAll(u"--open-all"_s) > 0;
    const bool automatic = args.removeAll(u"--auto"_s) > 0;
    QString mode = args.value(0, u"frame"_s);
    if (mode == u"frame") {
        mode = u"chooser"_s;
    } else if (mode == u"focus") {
        mode = u"trigger"_s;
    }
    SpikeHelper::setMode(mode);

    QString appId = u"io.github.strandzen.KwikLook.Spike"_s;
    if (mode == u"notitle") {
        appId += u".NoTitle"_s; // what the Breeze override matches
    } else if (mode == u"viewer") {
        appId += u".Viewer"_s;
        app.setQuitOnLastWindowClosed(false); // stays in the background like KwikLook's viewer
    }
    QGuiApplication::setDesktopFileName(appId);
    QApplication::setApplicationName(u"kwiklook-spike"_s);

    QQmlApplicationEngine engine;
    if (backdrop) {
        engine.loadFromModule("kwiklookspike", "Backdrop");
    }
    if (mode == u"chooser") {
        engine.setInitialProperties({{u"openAll"_s, openAll}});
        engine.loadFromModule("kwiklookspike", "Chooser");
    } else if (mode == u"trigger") {
        engine.setInitialProperties({{u"automatic"_s, automatic}});
        engine.loadFromModule("kwiklookspike", "FocusTrigger");
    } else if (mode == u"viewer") {
        engine.loadFromModule("kwiklookspike", "FocusViewer");
    } else if (mode == u"native" || mode == u"notitle" || mode == u"frameless") {
        engine.loadFromModule("kwiklookspike", "SampleWindow");
    } else {
        qWarning("Unknown mode %s. Use: frame | focus", qPrintable(mode));
        return 2;
    }
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    return app.exec();
}
