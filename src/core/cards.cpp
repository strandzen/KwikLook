// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "cards.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QLibrary>
#include <QLoggingCategory>
#include <QMutex>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcCards, "kwiklook.cards")

namespace
{
using ReadCards = int (*)(const QByteArray *, const QString *, int, QVariantList *);

QMutex mutex;
QString overridePath;
bool tried = false;
ReadCards readCards = nullptr;

// Next to the binary in the build tree, else where it's installed.
QString libraryPath()
{
    if (!overridePath.isEmpty()) {
        return overridePath;
    }
    const QString local = QCoreApplication::applicationDirPath() + u"/kwiklook-cards.so"_s;
    return QFileInfo::exists(local) ? local : QStringLiteral(KWIKLOOK_CARDS_LIBRARY);
}

ReadCards load()
{
    QMutexLocker lock(&mutex);
    if (!tried) {
        tried = true;
        QLibrary library(libraryPath());
        // Resolved once and kept loaded for the program's life.
        readCards = reinterpret_cast<ReadCards>(library.resolve("kwiklook_read_cards_v1"));
        if (!readCards) {
            qCInfo(lcCards) << "contact and calendar cards off:" << library.errorString();
        }
    }
    return readCards;
}
}

bool Cards::available()
{
    return load() != nullptr;
}

int Cards::read(const QByteArray &data, const QString &kind, int limit, QVariantList *cards)
{
    const ReadCards function = load();
    return function ? function(&data, &kind, limit, cards) : 0;
}

void Cards::setLibraryPath(const QString &path)
{
    QMutexLocker lock(&mutex);
    overridePath = path;
    tried = false;
    readCards = nullptr;
}
