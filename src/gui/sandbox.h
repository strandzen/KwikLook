// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QString>

// kwiklook-sandbox (src/sandbox): runs a converter locked in. Used for LibreOffice
// (OfficeConverter) and the archive lister (ArchiveModel).
namespace Sandbox
{
// Its exit code when the kernel's Landlock is too old to lock a program in.
constexpr int ExitNoSandbox = 77;

// Next to the binary in the build tree, else where it's installed; empty if missing.
QString binary();

// The helper is there and this kernel can lock a program in (asked once).
bool supported();
}
