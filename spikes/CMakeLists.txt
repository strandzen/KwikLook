# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
# Throwaway experiments for decisions in Todo.md. Never installed. See README.md.

add_executable(kwiklook-spike main.cpp)
qt_add_qml_module(kwiklook-spike
    URI "kwiklookspike"
    VERSION 1.0
    QML_FILES Chooser.qml SampleWindow.qml FocusTrigger.qml FocusViewer.qml Backdrop.qml
    SOURCES spikehelper.h spikehelper.cpp
    RESOURCE_PREFIX "/qt/qml"
    NO_PLUGIN
)
target_link_libraries(kwiklook-spike PRIVATE
    Qt6::Widgets Qt6::Quick Qt6::DBus
    KF6::WindowSystem
)
