// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>

// The Dolphin functions the hook calls, checked against the installed Dolphin
// (and against the stand-in Dolphin of tst_dolphinhook). In CI a current Dolphin
// is installed and KWIKLOOK_REQUIRE_DOLPHIN=1 is set, so a Dolphin release that
// renames or re-types one of them turns CI red; elsewhere a missing Dolphin
// skips the test.

#include "dolphinsymbols.h"

#include <QFileInfo>
#include <QTest>

#include <dlfcn.h>

using namespace Qt::StringLiterals;

class TstDolphinSymbols : public QObject
{
    Q_OBJECT
private:
    // Every Required and Optional symbol, or for an optional one its older form.
    static QStringList missingIn(void *library)
    {
        using DolphinSymbols::Need;
        const auto has = [library](const char *name) {
            return ::dlsym(library, name) != nullptr;
        };
        QStringList missing;
        for (const auto &symbol : DolphinSymbols::All) {
            if (symbol.need == Need::Older || has(symbol.name)) {
                continue;
            }
            bool olderFound = false;
            for (const auto &older : DolphinSymbols::All) {
                olderFound = olderFound || (older.need == Need::Older && qstrcmp(older.newer, symbol.name) == 0 && has(older.name));
            }
            if (!olderFound) {
                missing << QString::fromLatin1(symbol.name);
            }
        }
        return missing;
    }

private Q_SLOTS:
    void installedDolphinHasThem()
    {
        void *library = ::dlopen("libdolphinprivate.so.6", RTLD_LAZY | RTLD_LOCAL);
        if (!library) {
            const QByteArray error = ::dlerror();
            if (qEnvironmentVariableIntValue("KWIKLOOK_REQUIRE_DOLPHIN") != 0) {
                QFAIL(qPrintable(u"Dolphin's library not found: "_s + QString::fromLocal8Bit(error)));
            }
            QSKIP("Dolphin isn't installed");
        }
        // Which Dolphin this is (the file behind the soname carries the release).
        Dl_info info{};
        if (::dladdr(::dlsym(library, DolphinSymbols::ModelCount.name), &info) && info.dli_fname) {
            qInfo() << "checking" << QFileInfo(QString::fromLocal8Bit(info.dli_fname)).canonicalFilePath();
        }
        const QStringList missing = missingIn(library);
        QVERIFY2(missing.isEmpty(), qPrintable(u"not in the installed Dolphin: "_s + missing.join(u", ")));
        // Never dlclose() it: unloading Dolphin's library in a running Qt
        // application crashed (its static objects); the process ends soon anyway.
    }

    void standInDolphinHasThem()
    {
        void *library = ::dlopen(KWIKLOOK_MOCK_DOLPHIN, RTLD_LAZY | RTLD_LOCAL);
        QVERIFY2(library, ::dlerror());
        const QStringList missing = missingIn(library);
        QVERIFY2(missing.isEmpty(), qPrintable(u"not in tests/dolphin/mockdolphin: "_s + missing.join(u", ")));
    }
};

QTEST_GUILESS_MAIN(TstDolphinSymbols)
#include "tst_dolphinsymbols.moc"
