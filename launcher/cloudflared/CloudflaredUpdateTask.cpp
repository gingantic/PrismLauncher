// SPDX-License-Identifier: GPL-3.0-only
#include "CloudflaredUpdateTask.h"

#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>

#include "FileSystem.h"
#include "net/ApiDownload.h"
#include "net/NetUtils.h"
#include "settings/SettingsObject.h"

namespace {
constexpr const char* kCloudflaredLatestUrl = "https://api.github.com/repos/cloudflare/cloudflared/releases/latest";

QString findWindowsAssetName(const QJsonArray& assets)
{
    const auto arch = QSysInfo::currentCpuArchitecture().toLower();
    QStringList preferredNames;
    if (arch.contains("arm64")) {
        preferredNames << "cloudflared-windows-arm64.exe";
    } else if (arch.contains("86") && !arch.contains("64")) {
        preferredNames << "cloudflared-windows-386.exe";
    } else {
        preferredNames << "cloudflared-windows-amd64.exe";
    }

    for (const auto& preferredName : preferredNames) {
        for (const auto& assetValue : assets) {
            auto asset = assetValue.toObject();
            const auto name = asset.value("name").toString();
            if (name == preferredName) {
                return name;
            }
        }
    }

    return {};
}

QUrl findAssetUrl(const QJsonArray& assets, const QString& name)
{
    for (const auto& assetValue : assets) {
        auto asset = assetValue.toObject();
        if (asset.value("name").toString() == name) {
            return QUrl(asset.value("browser_download_url").toString());
        }
    }
    return {};
}
}  // namespace

CloudflaredUpdateTask::CloudflaredUpdateTask(SettingsObject* settings, QNetworkAccessManager* network, QString dataRoot)
    : Task(), m_settings(settings), m_network(network), m_dataRoot(std::move(dataRoot))
{
}

void CloudflaredUpdateTask::executeTask()
{
    setStatus(tr("Checking cloudflared updates"));

    auto [request, response] = Net::ApiDownload::makeByteArray(QUrl(kCloudflaredLatestUrl));
    m_request = request;

    m_job.reset(new NetJob("CloudflaredMetadata", m_network));
    m_job->setAskRetry(false);
    m_job->addNetAction(m_request);

    connect(m_job.get(), &Task::finished, this, [this, response] { onMetadataDone(response); });
    m_job->start();
}

void CloudflaredUpdateTask::onMetadataDone(QByteArray* response)
{
    if (m_request->error() != QNetworkReply::NoError) {
        auto message = tr("Failed to query cloudflared releases: %1").arg(m_request->errorString());
        setLastChecked();
        emitFailed(message);
        return;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(*response, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        setLastChecked();
        emitFailed(tr("Cloudflared metadata could not be parsed"));
        return;
    }

    auto obj = doc.object();
    m_latestTag = obj.value("tag_name").toString();
    auto assets = obj.value("assets").toArray();
    m_assetName = findWindowsAssetName(assets);
    if (m_assetName.isEmpty()) {
        setLastChecked();
        emitFailed(tr("No cloudflared Windows asset found in the latest release"));
        return;
    }

    m_assetUrl = findAssetUrl(assets, m_assetName);
    if (!m_assetUrl.isValid()) {
        setLastChecked();
        emitFailed(tr("Cloudflared release asset URL is invalid"));
        return;
    }

    const auto baseDir = FS::PathCombine(m_dataRoot, "libraries", "cloudflared");
    if (!FS::ensureFolderPathExists(baseDir)) {
        setLastChecked();
        emitFailed(tr("Failed to create cloudflared directory"));
        return;
    }
    m_targetPath = FS::PathCombine(baseDir, m_assetName);

    const auto storedTag = m_settings->get("CloudflaredLatestTag").toString();
    if (!m_latestTag.isEmpty() && storedTag == m_latestTag && QFileInfo(m_targetPath).isFile()) {
        m_settings->set("CloudflaredBinaryPath", m_targetPath);
        setLastChecked();
        setStatus(tr("Cloudflared is up to date"));
        emitSucceeded();
        return;
    }

    setStatus(tr("Downloading cloudflared"));

    m_request = Net::ApiDownload::makeFile(m_assetUrl, m_targetPath);
    m_job.reset(new NetJob("CloudflaredDownload", m_network));
    m_job->setAskRetry(false);
    m_job->addNetAction(m_request);

    connect(m_job.get(), &Task::finished, this, &CloudflaredUpdateTask::onDownloadDone);
    m_job->start();
}

void CloudflaredUpdateTask::onDownloadDone()
{
    if (m_request->error() != QNetworkReply::NoError) {
        auto message = tr("Failed to download cloudflared: %1").arg(m_request->errorString());
        setLastChecked();
        emitFailed(message);
        return;
    }

    if (!m_latestTag.isEmpty()) {
        m_settings->set("CloudflaredLatestTag", m_latestTag);
    }
    m_settings->set("CloudflaredBinaryPath", m_targetPath);
    setLastChecked();
    setStatus(tr("Cloudflared updated"));
    emitSucceeded();
}

void CloudflaredUpdateTask::setLastChecked()
{
    m_settings->set("CloudflaredLastChecked", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
}
