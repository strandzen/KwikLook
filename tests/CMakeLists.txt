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
if(TARGET kwiklook-cards)
    add_dependencies(tst_qmlsmoke kwiklook-cards) # the cards view
endif()

# Viewer state and signals (no window, no bus).
ecm_add_test(tst_viewer.cpp
    TEST_NAME tst_viewer
    LINK_LIBRARIES Qt6::Test kwiklook_qml
    NAME_PREFIX "kwiklook-"
)

# The geometry of the KWin script (data/kwin/kwiklook-placement.js), in a QJSEngine.
ecm_add_test(tst_placement.cpp
    TEST_NAME tst_placement
    LINK_LIBRARIES Qt6::Test Qt6::Qml
    NAME_PREFIX "kwiklook-"
)
target_compile_definitions(tst_placement PRIVATE PLACEMENT_SCRIPT="${PROJECT_SOURCE_DIR}/data/kwin/kwiklook-placement.js")
# Breeze settings and the shadow made from them (checked against measured Breeze shadows).
ecm_add_test(tst_breezestyle.cpp
    TEST_NAME tst_breezestyle
    LINK_LIBRARIES Qt6::Test kwiklook_core
    NAME_PREFIX "kwiklook-"
)

# Archive listings: kwiklook-archive's output read back, and the helper on real archives.
ecm_add_test(tst_archivelisting.cpp
    TEST_NAME tst_archivelisting
    LINK_LIBRARIES Qt6::Test kwiklook_core
    NAME_PREFIX "kwiklook-"
)
target_compile_definitions(tst_archivelisting PRIVATE ARCHIVE_HELPER="$<TARGET_FILE:kwiklook-archive>")
add_dependencies(tst_archivelisting kwiklook-archive)

# EPUB books: the helper hands over the files, Epub::build makes the document.
ecm_add_test(tst_epub.cpp
    TEST_NAME tst_epub
    LINK_LIBRARIES Qt6::Test kwiklook_core
    NAME_PREFIX "kwiklook-"
)
target_compile_definitions(tst_epub PRIVATE ARCHIVE_HELPER="$<TARGET_FILE:kwiklook-archive>")
add_dependencies(tst_epub kwiklook-archive)

# Markdown for Qt's reader (empty HTML elements closed).
ecm_add_test(tst_markdown.cpp
    TEST_NAME tst_markdown
    LINK_LIBRARIES Qt6::Test kwiklook_core
    NAME_PREFIX "kwiklook-"
)

# Headings of Markdown and HTML for the contents sidebar.
ecm_add_test(tst_headings.cpp
    TEST_NAME tst_headings
    LINK_LIBRARIES Qt6::Test kwiklook_core
    NAME_PREFIX "kwiklook-"
)

# File details (type, size, date) and which view previews a file.
ecm_add_test(tst_fileinfo.cpp
    TEST_NAME tst_fileinfo
    LINK_LIBRARIES Qt6::Test kwiklook_core
    NAME_PREFIX "kwiklook-"
)
if(TARGET kwiklook-cards)
    add_dependencies(tst_fileinfo kwiklook-cards)
endif()

# The Dolphin hook inside a stand-in Dolphin (KDEGlance's mock of Dolphin's view
# classes), talking to a stand-in viewer on a private bus.
add_library(mockdolphinprivate SHARED dolphin/mockdolphin.cpp)
target_link_libraries(mockdolphinprivate PUBLIC Qt6::Widgets KF6::KIOCore)
# Exported like libdolphinprivate: the hook looks the functions up by name.
set_target_properties(mockdolphinprivate PROPERTIES CXX_VISIBILITY_PRESET default VISIBILITY_INLINES_HIDDEN OFF)
add_executable(tst_dolphinhook dolphin/tst_dolphinhook.cpp)
target_include_directories(tst_dolphinhook PRIVATE ${CMAKE_SOURCE_DIR}/src/core dolphin)
target_link_libraries(tst_dolphinhook PRIVATE mockdolphinprivate Qt6::Test Qt6::DBus KF6::KIOCore KF6::ConfigCore)
target_compile_definitions(tst_dolphinhook PRIVATE KWIKLOOK_HOOK_PLUGIN="$<TARGET_FILE:kwiklook_dolphinhook>")
add_dependencies(tst_dolphinhook kwiklook_dolphinhook)
add_test(NAME kwiklook-tst_dolphinhook COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run-with-bus.sh $<TARGET_FILE:tst_dolphinhook>)

# kwiklook-sandbox: a probe program run inside it tries each thing it must not do.
add_executable(sandboxprobe sandboxprobe.cpp)
ecm_add_test(tst_sandbox.cpp
    TEST_NAME tst_sandbox
    LINK_LIBRARIES Qt6::Test
    NAME_PREFIX "kwiklook-"
)
target_compile_definitions(tst_sandbox PRIVATE
    SANDBOX_BINARY="$<TARGET_FILE:kwiklook-sandbox>"
    PROBE_BINARY="$<TARGET_FILE:sandboxprobe>")
add_dependencies(tst_sandbox kwiklook-sandbox sandboxprobe)

# Office previews: LibreOffice in kwiklook-sandbox, the cache. Skipped without LibreOffice.
ecm_add_test(tst_officeconverter.cpp
    TEST_NAME tst_officeconverter
    LINK_LIBRARIES Qt6::Test kwiklook_qml
    NAME_PREFIX "kwiklook-"
)
add_dependencies(tst_officeconverter kwiklook-sandbox)
