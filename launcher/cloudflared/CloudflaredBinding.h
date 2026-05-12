// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QJsonObject>
#include <QString>

struct CloudflaredBinding {
    enum class Type {
        Expose,  // run cloudflared tunnel to expose a local port via a public cloudflare URL
        Access,  // run cloudflared access to forward a cloudflare hostname to a local port
    };

    QString id;
    QString name;
    Type type = Type::Expose;
    // For Access: the cloudflare hostname (e.g. abc.trycloudflare.com)
    // For Expose: unused (URL is assigned by cloudflare at runtime)
    QString hostname;
    int localPort = 25565;
    bool autoStart = false;

    static CloudflaredBinding create();
    QJsonObject toJson() const;
    static CloudflaredBinding fromJson(const QJsonObject& obj);
};
