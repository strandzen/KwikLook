// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "hookguard.h"

#include "version.h"

#include <KConfigGroup>

#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace
{
constexpr auto Group = "DolphinHook";
constexpr auto VersionsKey = "Versions";
constexpr auto CheckingKey = "Checking";
constexpr auto PassedKey = "Passed";
constexpr auto BrokenKey = "Broken";
}

HookGuard::HookGuard(const QString &file, const QString &versions)
    : m_config(KSharedConfig::openConfig(file, KConfig::SimpleConfig))
{
    KConfigGroup group = m_config->group(QLatin1String(Group));
    if (group.readEntry(VersionsKey, QString()) != versions) {
        // Another Dolphin or KwikLook: what was learned no longer holds.
        group.deleteGroup();
        group = m_config->group(QLatin1String(Group));
        group.writeEntry(VersionsKey, versions);
        m_config->sync();
        return;
    }
    const QString checking = group.readEntry(CheckingKey, QString());
    if (!checking.isEmpty()) {
        group.writeEntry(BrokenKey, u"Dolphin stopped during KwikLook's first “%1” step under these versions"_s.arg(checking));
        group.deleteEntry(CheckingKey);
        m_config->sync();
    }
    m_broken = group.readEntry(BrokenKey, QString());
    m_passed = group.readEntry(PassedKey, QStringList());
}

QString HookGuard::defaultFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + u"/kwiklookstaterc"_s;
}

QString HookGuard::versionsKey(const QString &dolphinVersion)
{
    return u"Dolphin %1, KwikLook %2"_s.arg(dolphinVersion, QStringLiteral(KWIKLOOK_VERSION_STRING));
}

QString HookGuard::broken() const
{
    return m_broken;
}

bool HookGuard::begin(const QString &name)
{
    if (m_passed.contains(name)) {
        return false;
    }
    m_config->group(QLatin1String(Group)).writeEntry(CheckingKey, name);
    m_config->sync(); // on disk before the call that may not return
    return true;
}

void HookGuard::end(const QString &name, bool done)
{
    KConfigGroup group = m_config->group(QLatin1String(Group));
    group.deleteEntry(CheckingKey);
    if (done) {
        m_passed.append(name);
        group.writeEntry(PassedKey, m_passed);
    }
    m_config->sync();
}
