// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QString>
#include <QVariantList>

/*
 * Contacts (.vcf) and events (.ics) as cards, read by kwiklook-cards.so (src/cards),
 * which KwikLook opens the first time it's needed. Only that library links KContacts
 * and KCalendarCore, so both are optional: without them available() is false and the
 * files show as text. Safe to call from any thread.
 */
namespace Cards
{
bool available();

// Contacts ("vcard") or events ("calendar") from `data`, at most `limit` cards;
// returns how many the file has (also those beyond the limit). 0 when unavailable.
int read(const QByteArray &data, const QString &kind, int limit, QVariantList *cards);

// For tests: use this file instead (a missing one: as if not installed).
void setLibraryPath(const QString &path);
}
