# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
include(ECMAddTests)

# Loads every top-level QML window offscreen and checks it builds without errors.
ecm_add_test(tst_qmlsmoke.cpp
    TEST_NAME tst_qmlsmoke
    LINK_LIBRARIES Qt6::Test Qt6::Widgets kwiklook_qml kwiklook_qmlplugin KF6::I18n
    NAME_PREFIX "kwiklook-"
)
set_tests_properties(kwiklook-tst_qmlsmoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_CONTROLS_STYLE=org.kde.desktop")

# Viewer state and signals (no window, no bus).
ecm_add_test(tst_viewer.cpp
    TEST_NAME tst_viewer
    LINK_LIBRARIES Qt6::Test kwiklook_qml
    NAME_PREFIX "kwiklook-"
)

# Breeze settings and the shadow made from them (checked against measured Breeze shadows).
ecm_add_test(tst_breezestyle.cpp
    TEST_NAME tst_breezestyle
    LINK_LIBRARIES Qt6::Test kwiklook_core
    NAME_PREFIX "kwiklook-"
)
