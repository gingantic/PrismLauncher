// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QNetworkAccessManager>

#include "QObjectPtr.h"
#include "net/Download.h"
#include "net/NetJob.h"
#include "tasks/Task.h"

class SettingsObject;

class CloudflaredUpdateTask : public Task {
    Q_OBJECT

   public:
    explicit CloudflaredUpdateTask(SettingsObject* settings, QNetworkAccessManager* network, QString dataRoot);

    QString latestTag() const { return m_latestTag; }

   protected:
    void executeTask() override;

   private slots:
    void onMetadataDone();
    void onDownloadDone();

   private:
    void setLastChecked();

    SettingsObject* m_settings = nullptr;
    QNetworkAccessManager* m_network = nullptr;
    QString m_dataRoot;

    NetJob::Ptr m_job;
    Net::Download::Ptr m_request;
    std::unique_ptr<QByteArray> m_response;

    QString m_latestTag;
    QString m_assetName;
    QUrl m_assetUrl;
    QString m_targetPath;
};
