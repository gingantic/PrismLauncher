// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QMap>
#include <QObject>
#include <QProcess>
#include <QVector>

#include "CloudflaredBinding.h"

class SettingsObject;

class CloudflaredManager : public QObject {
    Q_OBJECT
   public:
    enum class Status { Stopped, Starting, Running, Error };
    Q_ENUM(Status)

    explicit CloudflaredManager(SettingsObject* settings, QObject* parent = nullptr);
    ~CloudflaredManager() override;

    const QVector<CloudflaredBinding>& bindings() const { return m_bindings; }

    void addBinding(const CloudflaredBinding& b);
    void removeBinding(const QString& id);
    void updateBinding(const CloudflaredBinding& b);

    void startBinding(const QString& id);
    void stopBinding(const QString& id);
    void stopAll();

    Status statusOf(const QString& id) const;
    QString tunnelUrlOf(const QString& id) const;

    void saveBindings();

   signals:
    void bindingsChanged();
    void bindingStatusChanged(const QString& id, CloudflaredManager::Status status, const QString& tunnelUrl);
    void bindingOutput(const QString& id, const QString& text);

   private:
    void loadBindings();
    QString binaryPath() const;
    int bindingIndex(const QString& id) const;

    SettingsObject* m_settings;
    QVector<CloudflaredBinding> m_bindings;

    struct Entry {
        QProcess* process = nullptr;
        Status status = Status::Stopped;
        QString tunnelUrl;
    };
    QMap<QString, Entry> m_processes;
};
