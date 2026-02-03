// SPDX-License-Identifier: GPL-3.0-only
#include "AuthlibInjectorUpdateTask.h"

#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "Application.h"
#include "FileSystem.h"
#include "net/ApiDownload.h"
#include "net/NetUtils.h"
#include "settings/SettingsObject.h"

namespace {
constexpr const char* kAuthlibInjectorLatestUrl = "https://api.github.com/repos/yushijinhun/authlib-injector/releases/latest";

QString findJarAssetName(const QJsonArray& assets)
{
    for (const auto& assetValue : assets) {
        auto asset = assetValue.toObject();
        const auto name = asset.value("name").toString();
        if (name.endsWith(".jar", Qt::CaseInsensitive)) {
            return name;
        }
    }
    return {};
}

QUrl findJarAssetUrl(const QJsonArray& assets, const QString& name)
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

AuthlibInjectorUpdateTask::AuthlibInjectorUpdateTask(SettingsObject* settings, QNetworkAccessManager* network, QString dataRoot)
    : Task(), m_settings(settings), m_network(network), m_dataRoot(std::move(dataRoot))
{
}

void AuthlibInjectorUpdateTask::executeTask()
{
    setStatus(tr("Checking authlib-injector updates"));

    m_response.reset(new QByteArray());
    m_request = Net::ApiDownload::makeByteArray(QUrl(kAuthlibInjectorLatestUrl), m_response.get());

    m_job.reset(new NetJob("AuthlibInjectorMetadata", m_network));
    m_job->setAskRetry(false);
    m_job->addNetAction(m_request);

    connect(m_job.get(), &Task::finished, this, &AuthlibInjectorUpdateTask::onMetadataDone);
    m_job->start();
}

void AuthlibInjectorUpdateTask::onMetadataDone()
{
    if (m_request->error() != QNetworkReply::NoError) {
        auto message = tr("Failed to query authlib-injector releases: %1").arg(m_request->errorString());
        setLastChecked();
        emitFailed(message);
        return;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(*m_response, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        setLastChecked();
        emitFailed(tr("Authlib-injector metadata could not be parsed"));
        return;
    }

    auto obj = doc.object();
    m_latestTag = obj.value("tag_name").toString();
    auto assets = obj.value("assets").toArray();
    m_assetName = findJarAssetName(assets);
    if (m_assetName.isEmpty()) {
        setLastChecked();
        emitFailed(tr("No authlib-injector JAR asset found in the latest release"));
        return;
    }

    m_assetUrl = findJarAssetUrl(assets, m_assetName);
    if (!m_assetUrl.isValid()) {
        setLastChecked();
        emitFailed(tr("Authlib-injector release asset URL is invalid"));
        return;
    }

    const auto baseDir = FS::PathCombine(m_dataRoot, "libraries", "authlib-injector");
    if (!FS::ensureFolderPathExists(baseDir)) {
        setLastChecked();
        emitFailed(tr("Failed to create authlib-injector directory"));
        return;
    }
    m_targetPath = FS::PathCombine(baseDir, m_assetName);

    const auto storedTag = m_settings->get("AuthlibInjectorLatestTag").toString();
    if (!m_latestTag.isEmpty() && storedTag == m_latestTag && QFileInfo(m_targetPath).isFile()) {
        m_settings->set("AuthlibInjectorJarPath", m_targetPath);
        setLastChecked();
        setStatus(tr("Authlib-injector is up to date"));
        emitSucceeded();
        return;
    }

    setStatus(tr("Downloading authlib-injector"));

    m_request = Net::ApiDownload::makeFile(m_assetUrl, m_targetPath);
    m_job.reset(new NetJob("AuthlibInjectorDownload", m_network));
    m_job->setAskRetry(false);
    m_job->addNetAction(m_request);

    connect(m_job.get(), &Task::finished, this, &AuthlibInjectorUpdateTask::onDownloadDone);
    m_job->start();
}

void AuthlibInjectorUpdateTask::onDownloadDone()
{
    if (m_request->error() != QNetworkReply::NoError) {
        auto message = tr("Failed to download authlib-injector: %1").arg(m_request->errorString());
        setLastChecked();
        emitFailed(message);
        return;
    }

    if (!m_latestTag.isEmpty()) {
        m_settings->set("AuthlibInjectorLatestTag", m_latestTag);
    }
    m_settings->set("AuthlibInjectorJarPath", m_targetPath);
    setLastChecked();
    setStatus(tr("Authlib-injector updated"));
    emitSucceeded();
}

void AuthlibInjectorUpdateTask::setLastChecked()
{
    m_settings->set("AuthlibInjectorLastChecked", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
}
