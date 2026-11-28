// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QKeyCombination>
#include <QKeySequence>
#include <QStringList>

/*
 * The key that opens (and closes) the preview: kwiklookrc [General] OpenKey, read by
 * the viewer and by the Dolphin hook. Only a few combinations are offered: a plain
 * letter would break Dolphin's type-ahead search and renaming.
 */
namespace OpenKey
{
inline constexpr auto ConfigFile = "kwiklookrc";
inline constexpr auto ConfigGroup = "General";
inline constexpr auto ConfigKey = "OpenKey";

// In QKeySequence's portable text, as stored in the config file.
inline QStringList choices()
{
    return {QStringLiteral("Space"), QStringLiteral("Shift+Space"), QStringLiteral("Ctrl+Space")};
}

// The stored text, or Space when it's missing or not one of choices().
inline QString normalized(const QString &text)
{
    return choices().contains(text) ? text : choices().constFirst();
}

inline QKeyCombination combination(const QString &text)
{
    return QKeySequence::fromString(normalized(text), QKeySequence::PortableText)[0];
}

// The key and modifiers of a key event, as a combination to compare with combination().
inline QKeyCombination eventCombination(Qt::Key key, Qt::KeyboardModifiers modifiers)
{
    return QKeyCombination(modifiers & ~Qt::KeypadModifier, key);
}
} // namespace OpenKey
