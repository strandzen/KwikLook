# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>

# QML module: C++ QML types and every file in qml/. Linked by the binary and the tests.
set(KWIKLOOK_QML_FILES
    ${CMAKE_SOURCE_DIR}/qml/Main.qml
    ${CMAKE_SOURCE_DIR}/qml/TitleBar.qml
    ${CMAKE_SOURCE_DIR}/qml/ResizeHandles.qml
    ${CMAKE_SOURCE_DIR}/qml/FallbackView.qml
    ${CMAKE_SOURCE_DIR}/qml/FileThumbnail.qml
    ${CMAKE_SOURCE_DIR}/qml/ImageView.qml
    ${CMAKE_SOURCE_DIR}/qml/InfoPanel.qml
    ${CMAKE_SOURCE_DIR}/qml/TextView.qml
    ${CMAKE_SOURCE_DIR}/qml/PdfView.qml
    ${CMAKE_SOURCE_DIR}/qml/MediaView.qml
    ${CMAKE_SOURCE_DIR}/qml/FontView.qml
    ${CMAKE_SOURCE_DIR}/qml/FindBar.qml
    ${CMAKE_SOURCE_DIR}/qml/BarItems.qml
    ${CMAKE_SOURCE_DIR}/qml/BarEditor.qml
    ${CMAKE_SOURCE_DIR}/qml/DesktopView.qml
    ${CMAKE_SOURCE_DIR}/qml/SettingsWindow.qml
)

# Pure logic, no Qt Quick: Breeze settings, shadow images, file details.
add_library(kwiklook_core STATIC
    core/breezestyle.cpp
    core/shadowimage.cpp
    core/fileinfo.cpp
)
target_include_directories(kwiklook_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/core)
target_link_libraries(kwiklook_core PUBLIC Qt6::Gui KF6::ConfigCore KF6::ConfigGui) # ConfigGui: QColor entries
target_link_libraries(kwiklook_core PRIVATE KF6::FileMetaData) # details like EXIF, page count
set_source_files_properties(${CMAKE_SOURCE_DIR}/qml/BarItems.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
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
        gui/thumbnailprovider.h
        gui/thumbnailprovider.cpp
        gui/linenumbers.h
        gui/linenumbers.cpp
        gui/autostart.h
        gui/autostart.cpp
        gui/officeconverter.h
        gui/officeconverter.cpp
        gui/offlinenetwork.h
        gui/offlinenetwork.cpp
    RESOURCE_PREFIX "/qt/qml" # Qt searches here by default
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/kwiklook"
)
# Where kwiklook-sandbox is installed (the build tree's copy is found next to the binary).
target_compile_definitions(kwiklook_qml PRIVATE KWIKLOOK_LIBEXECDIR="${KDE_INSTALL_FULL_LIBEXECDIR}")
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
    KF6::ConfigCore # settings
    Qt6::Concurrent # reading file details off the main thread
    Qt6::Network # gui/offlinenetwork
    KF6::KIOWidgets # “Open with…”: applications for a file, the “Other Application” dialog
    KF6::Service
    KF6::Purpose # Share
    KF6::PurposeWidgets
    Qt6::Widgets # the menus are QMenus, like Dolphin's
)

# Settings (KConfig XT): the generated KwikLookSettings is also a QML singleton.
kconfig_add_kcfg_files(kwiklook_qml GENERATE_MOC gui/kwiklooksettings.kcfgc)

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

# The Dolphin hook: a plugin Dolphin loads into its own process (see src/dolphin/bridge.h).
add_library(kwiklook_dolphinhook MODULE
    dolphin/hookplugin.cpp
    dolphin/bridge.cpp
    dolphin/dolphinaccess.cpp
)
target_include_directories(kwiklook_dolphinhook PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/core)
target_link_libraries(kwiklook_dolphinhook PRIVATE
    Qt6::Widgets
    Qt6::DBus
    KF6::KIOCore # KOverlayIconPlugin, KFileItem
    KF6::WindowSystem # activation token for the preview
    KF6::ConfigCore # the open key from kwiklookrc, and changes to it
    ${CMAKE_DL_LIBS} # Dolphin's internals, looked up by name
)
set_target_properties(kwiklook_dolphinhook PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/kf6/overlayicon")
install(TARGETS kwiklook_dolphinhook DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf6/overlayicon)

# Runs converters (LibreOffice) locked down: see sandbox/main.cpp. No Qt.
add_executable(kwiklook-sandbox sandbox/main.cpp)
target_link_libraries(kwiklook-sandbox PRIVATE PkgConfig::SECCOMP)
install(TARGETS kwiklook-sandbox DESTINATION ${KDE_INSTALL_LIBEXECDIR})
