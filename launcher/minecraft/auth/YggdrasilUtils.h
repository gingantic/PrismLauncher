// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QUrl>

QUrl buildAuthServerEndpointUrl(const QString& baseUrl, const QString& endpoint);
QUrl buildSessionServerProfileUrl(const QString& baseUrl, const QString& profileId);
