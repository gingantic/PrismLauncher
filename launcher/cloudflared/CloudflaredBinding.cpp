// SPDX-License-Identifier: GPL-3.0-only
#include "CloudflaredBinding.h"

#include <QUuid>

CloudflaredBinding CloudflaredBinding::create()
{
    CloudflaredBinding b;
    b.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return b;
}

QJsonObject CloudflaredBinding::toJson() const
{
    QJsonObject obj;
    obj["id"] = id;
    obj["name"] = name;
    obj["type"] = (type == Type::Expose) ? "expose" : "access";
    obj["hostname"] = hostname;
    obj["localPort"] = localPort;
    obj["autoStart"] = autoStart;
    return obj;
}

CloudflaredBinding CloudflaredBinding::fromJson(const QJsonObject& obj)
{
    CloudflaredBinding b;
    b.id = obj.value("id").toString();
    b.name = obj.value("name").toString();
    b.type = (obj.value("type").toString() == "access") ? Type::Access : Type::Expose;
    b.hostname = obj.value("hostname").toString();
    b.localPort = obj.value("localPort").toInt(25565);
    b.autoStart = obj.value("autoStart").toBool(false);
    return b;
}
