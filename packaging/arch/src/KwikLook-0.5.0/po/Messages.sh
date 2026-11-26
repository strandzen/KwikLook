#! /usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#
# Regenerate the POT template. Run from the repository root:
#   XGETTEXT=xgettext podir=po bash po/Messages.sh
# Add every new source file with user-visible strings to the lists below.

$XGETTEXT \
    src/main.cpp \
    src/gui/viewer.cpp \
    src/gui/filemenus.cpp \
    src/gui/autostart.cpp \
    src/gui/archivemodel.cpp \
    --keyword=i18n:1 \
    --keyword=i18nc:1c,2 \
    --keyword=i18np:1,2 \
    --keyword=i18ncp:1c,2,3 \
    --language=C++ \
    --from-code=UTF-8 \
    --add-comments=TRANSLATORS: \
    -o $podir/kwiklook.pot

$XGETTEXT \
    qml/Main.qml \
    qml/TitleBar.qml \
    qml/InfoPanel.qml \
    qml/TextView.qml \
    qml/MediaView.qml \
    qml/FontView.qml \
    qml/FindBar.qml \
    qml/PdfView.qml \
    qml/ContentsList.qml \
    qml/ArchiveView.qml \
    qml/BarItems.qml \
    qml/BarEditor.qml \
    qml/DesktopView.qml \
    qml/CsvView.qml \
    qml/CardsView.qml \
    qml/IndexSheet.qml \
    qml/SettingsWindow.qml \
    --keyword=i18n:1 \
    --keyword=i18nc:1c,2 \
    --keyword=i18np:1,2 \
    --keyword=i18ncp:1c,2,3 \
    --language=JavaScript \
    --from-code=UTF-8 \
    --add-comments=TRANSLATORS: \
    --join-existing \
    -o $podir/kwiklook.pot
