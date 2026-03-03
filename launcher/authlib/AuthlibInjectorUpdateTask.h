// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QNetworkAccessManager>

#include "QObjectPtr.h"
#include "net/Download.h"
#include "net/NetJob.h"
#include "tasks/Task.h"

class SettingsObject;

class AuthlibInjectorUpdateTask : public Task {
    Q_OBJECT

   public:
    explicit AuthlibInjectorUpdateTask(SettingsObject* settings, QNetworkAccessManager* network, QString dataRoot);

    QString latestTag() const { return m_latestTag; }

   protected:
    void executeTask() override;

   private slots:
    void onMetadataDone(QByteArray* response);
    void onDownloadDone();

   private:
    void setLastChecked();

    SettingsObject* m_settings = nullptr;
    QNetworkAccessManager* m_network = nullptr;
    QString m_dataRoot;

    NetJob::Ptr m_job;
    Net::Download::Ptr m_request;

    QString m_latestTag;
    QString m_assetName;
    QUrl m_assetUrl;
    QString m_targetPath;
};
