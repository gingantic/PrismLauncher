// SPDX-License-Identifier: GPL-3.0-only
#include "YggdrasilUtils.h"

QUrl buildAuthServerEndpointUrl(const QString& baseUrl, const QString& endpoint)
{
    QUrl url(baseUrl);
    if (!url.isValid())
        return url;

    QString path = url.path();
    if (path.endsWith("/")) {
        path.chop(1);
    }
    if (!path.endsWith("/authserver")) {
        path += "/authserver";
    }
    path += "/" + endpoint;
    url.setPath(path);
    return url;
}

QUrl buildSessionServerProfileUrl(const QString& baseUrl, const QString& profileId)
{
    QUrl url(baseUrl);
    if (!url.isValid())
        return url;

    QString path = url.path();
    if (path.endsWith("/")) {
        path.chop(1);
    }
    if (!path.endsWith("/sessionserver")) {
        path += "/sessionserver";
    }
    path += "/session/minecraft/profile/" + profileId;
    url.setPath(path);
    return url;
}
