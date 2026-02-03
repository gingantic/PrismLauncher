// SPDX-License-Identifier: GPL-3.0-only
#include "YggdrasilAuth.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUuid>

#include "Application.h"
#include "minecraft/auth/Parsers.h"
#include "minecraft/auth/YggdrasilUtils.h"
#include "net/NetUtils.h"
#include "net/RawHeaderProxy.h"

namespace {
QString parseErrorMessage(const QJsonObject& obj)
{
    const auto errorMessage = obj.value("errorMessage").toString();
    const auto cause = obj.value("cause").toString();
    if (!errorMessage.isEmpty() && !cause.isEmpty()) {
        return QString("%1 (%2)").arg(errorMessage, cause);
    }
    if (!errorMessage.isEmpty()) {
        return errorMessage;
    }
    return obj.value("error").toString();
}
}  // namespace

YggdrasilLoginStep::YggdrasilLoginStep(AccountData* data) : AuthStep(data) {}

QString YggdrasilLoginStep::describe()
{
    return tr("Authenticating with Yggdrasil");
}

void YggdrasilLoginStep::perform()
{
    if (m_data->yggdrasilServerUrl.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Yggdrasil server URL is missing"));
        return;
    }
    if (m_data->yggdrasilUserName.isEmpty() || m_data->yggdrasilPassword.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Username or password is missing"));
        return;
    }

    const auto clientToken = [&] {
        const auto existing = m_data->yggdrasilToken.extra.value("clientToken").toString();
        if (!existing.isEmpty()) {
            return existing;
        }
        static const QRegularExpression s_removeChars("[{}-]");
        return QUuid::createUuid().toString().remove(s_removeChars);
    }();
    m_data->yggdrasilToken.extra["clientToken"] = clientToken;

    QJsonObject agent;
    agent["name"] = "Minecraft";
    agent["version"] = 1;

    QJsonObject request;
    request["agent"] = agent;
    request["username"] = m_data->yggdrasilUserName;
    request["password"] = m_data->yggdrasilPassword;
    request["clientToken"] = clientToken;
    request["requestUser"] = true;

    QUrl url = buildAuthServerEndpointUrl(m_data->yggdrasilServerUrl, "authenticate");
    if (!url.isValid()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Yggdrasil server URL is invalid"));
        return;
    }

    auto headers = QList<Net::HeaderPair>{
        { "Content-Type", "application/json" },
        { "Accept", "application/json" },
    };

    m_response.reset(new QByteArray());
    m_request = Net::Upload::makeByteArray(url, m_response.get(), QJsonDocument(request).toJson(QJsonDocument::Compact));
    m_request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));

    m_task.reset(new NetJob("YggdrasilLoginStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, &YggdrasilLoginStep::onRequestDone);

    m_task->start();
}

void YggdrasilLoginStep::onRequestDone()
{
    m_data->yggdrasilPassword.clear();

    if (m_request->error() != QNetworkReply::NoError) {
        qWarning() << "Error authenticating with Yggdrasil:";
        qWarning() << " HTTP Status       :" << m_request->replyStatusCode();
        qWarning() << " Internal error no.:" << m_request->error();
        qWarning() << " Error string      :" << m_request->errorString();
        qWarning() << " Response          :" << QString::fromUtf8(*m_response);

        if (Net::isApplicationError(m_request->error())) {
            emit finished(AccountTaskState::STATE_FAILED_HARD, tr("Authentication failed: %1").arg(m_request->errorString()));
        } else {
            emit finished(AccountTaskState::STATE_OFFLINE, tr("Authentication server unreachable: %1").arg(m_request->errorString()));
        }
        return;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(*m_response, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Authentication response could not be parsed"));
        return;
    }

    auto obj = doc.object();
    if (obj.contains("error")) {
        auto message = parseErrorMessage(obj);
        if (message.isEmpty()) {
            message = tr("Authentication failed");
        }
        emit finished(AccountTaskState::STATE_FAILED_HARD, message);
        return;
    }

    const auto accessToken = obj.value("accessToken").toString();
    const auto clientToken = obj.value("clientToken").toString();
    const auto profile = obj.value("selectedProfile").toObject();
    const auto profileId = profile.value("id").toString();
    const auto profileName = profile.value("name").toString();
    const auto userObj = obj.value("user").toObject();
    const auto userName = userObj.value("username").toString();

    if (accessToken.isEmpty() || profileId.isEmpty() || profileName.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Authentication response was incomplete"));
        return;
    }

    m_data->yggdrasilToken.token = accessToken;
    m_data->yggdrasilToken.issueInstant = QDateTime::currentDateTimeUtc();
    m_data->yggdrasilToken.notAfter = QDateTime();
    m_data->yggdrasilToken.validity = Validity::Certain;
    if (!clientToken.isEmpty()) {
        m_data->yggdrasilToken.extra["clientToken"] = clientToken;
    }
    if (!userName.isEmpty()) {
        m_data->yggdrasilToken.extra["userName"] = userName;
    }

    m_data->minecraftProfile.id = profileId;
    m_data->minecraftProfile.name = profileName;
    m_data->minecraftProfile.validity = Validity::Certain;

    m_data->minecraftEntitlement.ownsMinecraft = true;
    m_data->minecraftEntitlement.canPlayMinecraft = true;
    m_data->minecraftEntitlement.validity = Validity::Certain;

    emit finished(AccountTaskState::STATE_WORKING, tr("Authenticated with Yggdrasil"));
}

YggdrasilRefreshStep::YggdrasilRefreshStep(AccountData* data) : AuthStep(data) {}

QString YggdrasilRefreshStep::describe()
{
    return tr("Refreshing Yggdrasil session");
}

void YggdrasilRefreshStep::perform()
{
    if (m_data->yggdrasilServerUrl.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Yggdrasil server URL is missing"));
        return;
    }
    if (m_data->yggdrasilToken.token.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_HARD, tr("Yggdrasil access token is missing"));
        return;
    }

    const auto clientToken = m_data->yggdrasilToken.extra.value("clientToken").toString();
    if (clientToken.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_HARD, tr("Yggdrasil client token is missing"));
        return;
    }

    QJsonObject request;
    request["accessToken"] = m_data->yggdrasilToken.token;
    request["clientToken"] = clientToken;
    request["requestUser"] = true;

    QUrl url = buildAuthServerEndpointUrl(m_data->yggdrasilServerUrl, "refresh");
    if (!url.isValid()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Yggdrasil server URL is invalid"));
        return;
    }

    auto headers = QList<Net::HeaderPair>{
        { "Content-Type", "application/json" },
        { "Accept", "application/json" },
    };

    m_response.reset(new QByteArray());
    m_request = Net::Upload::makeByteArray(url, m_response.get(), QJsonDocument(request).toJson(QJsonDocument::Compact));
    m_request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));

    m_task.reset(new NetJob("YggdrasilRefreshStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, &YggdrasilRefreshStep::onRequestDone);

    m_task->start();
}

void YggdrasilRefreshStep::onRequestDone()
{
    if (m_request->error() != QNetworkReply::NoError) {
        qWarning() << "Error refreshing Yggdrasil session:";
        qWarning() << " HTTP Status       :" << m_request->replyStatusCode();
        qWarning() << " Internal error no.:" << m_request->error();
        qWarning() << " Error string      :" << m_request->errorString();
        qWarning() << " Response          :" << QString::fromUtf8(*m_response);

        if (Net::isApplicationError(m_request->error())) {
            emit finished(AccountTaskState::STATE_FAILED_HARD, tr("Session refresh failed: %1").arg(m_request->errorString()));
        } else {
            emit finished(AccountTaskState::STATE_OFFLINE, tr("Authentication server unreachable: %1").arg(m_request->errorString()));
        }
        return;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(*m_response, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Session refresh response could not be parsed"));
        return;
    }

    auto obj = doc.object();
    if (obj.contains("error")) {
        auto message = parseErrorMessage(obj);
        if (message.isEmpty()) {
            message = tr("Session refresh failed");
        }
        emit finished(AccountTaskState::STATE_FAILED_HARD, message);
        return;
    }

    const auto accessToken = obj.value("accessToken").toString();
    const auto profile = obj.value("selectedProfile").toObject();
    const auto profileId = profile.value("id").toString();
    const auto profileName = profile.value("name").toString();

    if (accessToken.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Session refresh response was incomplete"));
        return;
    }

    m_data->yggdrasilToken.token = accessToken;
    m_data->yggdrasilToken.issueInstant = QDateTime::currentDateTimeUtc();
    m_data->yggdrasilToken.notAfter = QDateTime();
    m_data->yggdrasilToken.validity = Validity::Certain;

    if (!profileId.isEmpty() && !profileName.isEmpty()) {
        m_data->minecraftProfile.id = profileId;
        m_data->minecraftProfile.name = profileName;
        m_data->minecraftProfile.validity = Validity::Certain;
    }

    emit finished(AccountTaskState::STATE_WORKING, tr("Yggdrasil session refreshed"));
}

YggdrasilProfileStep::YggdrasilProfileStep(AccountData* data) : AuthStep(data) {}

QString YggdrasilProfileStep::describe()
{
    return tr("Fetching the Yggdrasil profile");
}

void YggdrasilProfileStep::perform()
{
    if (m_data->yggdrasilServerUrl.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Yggdrasil server URL is missing"));
        return;
    }

    const auto profileId = m_data->minecraftProfile.id;
    if (profileId.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Minecraft profile ID is missing"));
        return;
    }

    QUrl url = buildSessionServerProfileUrl(m_data->yggdrasilServerUrl, profileId);
    if (!url.isValid()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Yggdrasil server URL is invalid"));
        return;
    }

    m_response.reset(new QByteArray());
    m_request = Net::Download::makeByteArray(url, m_response.get());

    m_task.reset(new NetJob("YggdrasilProfileStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, &YggdrasilProfileStep::onRequestDone);

    m_task->start();
}

void YggdrasilProfileStep::onRequestDone()
{
    if (m_request->error() != QNetworkReply::NoError) {
        qWarning() << "Error getting Yggdrasil profile:";
        qWarning() << " HTTP Status       :" << m_request->replyStatusCode();
        qWarning() << " Internal error no.:" << m_request->error();
        qWarning() << " Error string      :" << m_request->errorString();
        qWarning() << " Response          :" << QString::fromUtf8(*m_response);

        if (Net::isApplicationError(m_request->error())) {
            emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Minecraft profile fetch failed: %1").arg(m_request->errorString()));
        } else {
            emit finished(AccountTaskState::STATE_OFFLINE, tr("Minecraft profile fetch failed: %1").arg(m_request->errorString()));
        }
        return;
    }

    MinecraftProfile profile = m_data->minecraftProfile;
    if (!Parsers::parseMinecraftProfileMojang(*m_response, profile)) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Minecraft profile response could not be parsed"));
        return;
    }

    m_data->minecraftProfile = profile;
    emit finished(AccountTaskState::STATE_WORKING, tr("Got Yggdrasil profile"));
}
