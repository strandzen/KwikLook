# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>

# QML module: C++ QML types and every file in qml/. Linked by the binary and the tests.
set(KWIKLOOK_QML_FILES
    ${CMAKE_SOURCE_DIR}/qml/Main.qml
    ${CMAKE_SOURCE_DIR}/qml/TitleBar.qml
    ${CMAKE_SOURCE_DIR}/qml/ResizeHandles.qml
)

# Pure logic, no Qt Quick: Breeze settings, shadow images.
add_library(kwiklook_core STATIC
    core/breezestyle.cpp
    core/shadowimage.cpp
)
target_include_directories(kwiklook_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/core)
target_link_libraries(kwiklook_core PUBLIC Qt6::Gui KF6::ConfigCore KF6::ConfigGui) # ConfigGui: QColor entries
foreach(qml_file ${KWIKLOOK_QML_FILES})
    get_filename_component(qml_name ${qml_file} NAME)
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_name})
endforeach()

# Static, so the app and the tests link the module (and its plugin) directly.
qt_add_library(kwiklook_qml STATIC)
qt_add_qml_module(kwiklook_qml
    URI "kwiklook"
    VERSION 1.0
    QML_FILES ${KWIKLOOK_QML_FILES}
    SOURCES
        gui/viewer.h
        gui/viewer.cpp
        gui/windowframe.h
        gui/windowframe.cpp
        gui/filemenus.h
        gui/filemenus.cpp
    RESOURCE_PREFIX "/qt/qml" # Qt searches here by default
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/kwiklook"
)
target_include_directories(kwiklook_qml PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/gui # QML type registration includes headers by bare name
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(kwiklook_qml PUBLIC
    kwiklook_core
    KF6::CoreAddons # KDirWatch
    Qt6::Quick
    Qt6::Qml
    Qt6::DBus
    KF6::I18nQml
    KF6::WindowSystem
    KF6::I18n
    KF6::KIOWidgets # “Open with…”: applications for a file, the “Other Application” dialog
    KF6::Service
    KF6::Purpose # Share
    KF6::PurposeWidgets
    Qt6::Widgets # the menus are QMenus, like Dolphin's
)

# The viewer's D-Bus interface (data/io.github.strandzen.KwikLook.Viewer.xml) is
# written by hand: qdbuscpp2xml also lists the non-scriptable currentChanged().

add_executable(kwiklook main.cpp)
target_link_libraries(kwiklook PRIVATE
    kwiklook_qml
    Qt6::QuickControls2
    kwiklook_qmlplugin
    Qt6::Widgets
    KF6::CoreAddons
    KF6::I18n
)
install(TARGETS kwiklook ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
