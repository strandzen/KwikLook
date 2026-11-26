# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
install(FILES io.github.strandzen.KwikLook.desktop io.github.strandzen.KwikLook.Settings.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES io.github.strandzen.KwikLook.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})

# D-Bus activation: the first request starts the background viewer.
configure_file(io.github.strandzen.KwikLook.service.in ${CMAKE_CURRENT_BINARY_DIR}/io.github.strandzen.KwikLook.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.strandzen.KwikLook.service DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR})
install(FILES io.github.strandzen.KwikLook.Viewer.xml DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR})
