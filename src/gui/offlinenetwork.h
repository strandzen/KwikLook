// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#pragma once

#include <QQmlNetworkAccessManagerFactory>

/*
 * Keeps the QML engine off the network: previewing a file never fetches anything.
 * Qt Quick text loads the pictures a document refers to (rendered Markdown, HTML in
 * it) through the engine's network access, including http(s) ones; with this factory
 * only local files load, other requests fail at once (nothing is sent).
 *
 *   engine.setNetworkAccessManagerFactory(OfflineNetworkFactory::instance());
 */
class OfflineNetworkFactory : public QQmlNetworkAccessManagerFactory
{
public:
    // Refused requests go to this (unknown) protocol, which names the reason in the log.
    static constexpr const char *BlockedScheme = "kwiklook-offline";

    static OfflineNetworkFactory *instance();
    QNetworkAccessManager *create(QObject *parent) override;
};
