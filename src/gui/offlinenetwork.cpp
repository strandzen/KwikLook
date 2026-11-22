// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 strandzen <strandzen@gmail.com>
#include "offlinenetwork.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>

using namespace Qt::Literals::StringLiterals;

namespace
{
class OfflineNetworkAccessManager : public QNetworkAccessManager
{
public:
    using QNetworkAccessManager::QNetworkAccessManager;

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData) override
    {
        const QString scheme = request.url().scheme();
        if (scheme == u"file"_s || scheme == u"qrc"_s || scheme == u"data"_s) {
            return QNetworkAccessManager::createRequest(op, request, outgoingData);
        }
        // A protocol Qt doesn't know: the reply fails at once and nothing is sent. Its
        // name is the explanation in the log: Protocol "kwiklook-offline" is unknown.
        QNetworkRequest blocked(request);
        blocked.setUrl(QUrl(QLatin1String(OfflineNetworkFactory::BlockedScheme) + u':' + request.url().toString()));
        return QNetworkAccessManager::createRequest(op, blocked, outgoingData);
    }
};
} // namespace

OfflineNetworkFactory *OfflineNetworkFactory::instance()
{
    static OfflineNetworkFactory factory;
    return &factory;
}

QNetworkAccessManager *OfflineNetworkFactory::create(QObject *parent)
{
    return new OfflineNetworkAccessManager(parent);
}
