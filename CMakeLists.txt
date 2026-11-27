# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
cmake_minimum_required(VERSION 3.22)
project(kwiklook VERSION 0.8.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(QT_MIN_VERSION 6.8.0)
set(KF_MIN_VERSION 6.19.0) # KWaylandExtras::xdgActivationToken()

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs6)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMSetupVersion)
include(FeatureSummary)

ecm_setup_version(${PROJECT_VERSION}
    VARIABLE_PREFIX KWIKLOOK
    VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/src/version.h"
)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Widgets     # QApplication, needed by the desktop QtQuick Controls style
    Quick
    QuickControls2 # selecting KDE's style
    Qml
    DBus
    Concurrent
    Pdf         # the contents (bookmarks) of PDFs
    Network     # the QML engine's network access, turned off (gui/offlinenetwork)
    Multimedia  # QML only (video and audio); checked here so a missing one shows at build time
)

find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    Config         # reading the Breeze settings
    CoreAddons     # KAboutData, KDirWatch
    Crash          # crash reports through DrKonqi
    I18n           # i18n() in C++ and QML
    WindowSystem   # Wayland activation tokens, compositor shadows
    KIO            # “Open with…”
    Service
    Purpose        # Share
    FileMetaData   # file details (EXIF, page count, ...)
)
# Contact and event cards (.vcf, .ics): only kwiklook-cards.so links them, so they are
# optional, at build time and at runtime (without them those files show as text).
find_package(KF6Contacts ${KF_MIN_VERSION})
find_package(KF6CalendarCore ${KF_MIN_VERSION})
find_package(KF6Kirigami ${KF_MIN_VERSION} REQUIRED)
find_package(LibArchive 3.6 REQUIRED) # kwiklook-archive: what's inside an archive
# kwiklook-sandbox: refuses sockets in the programs it runs (Landlock comes with the kernel headers).
find_package(PkgConfig REQUIRED)
pkg_check_modules(SECCOMP REQUIRED IMPORTED_TARGET libseccomp)
# QML only (form cards in the settings window); checked here so a missing one shows at build time.
find_package(KF6KirigamiAddons 1.0 REQUIRED)

add_subdirectory(src)
add_subdirectory(data)
# Fuzz targets for the parsers (stress/README.md): preset "fuzz", clang only.
option(KWIKLOOK_FUZZ "Build the libFuzzer targets in stress/fuzz (clang)" OFF)
if(KWIKLOOK_FUZZ)
    add_subdirectory(stress/fuzz)
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

ki18n_install(po)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
