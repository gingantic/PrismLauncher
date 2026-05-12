// SPDX-License-Identifier: GPL-3.0-only
#include "CloudflaredManager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include "settings/SettingsObject.h"

CloudflaredManager::CloudflaredManager(SettingsObject* settings, QObject* parent)
    : QObject(parent), m_settings(settings)
{
    loadBindings();
}

CloudflaredManager::~CloudflaredManager()
{
    stopAll();
}

void CloudflaredManager::loadBindings()
{
    const auto raw = m_settings->get("CloudflaredBindings").toString();
    if (raw.isEmpty())
        return;

    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(raw.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray())
        return;

    for (const auto& val : doc.array()) {
        if (val.isObject())
            m_bindings.append(CloudflaredBinding::fromJson(val.toObject()));
    }
}

void CloudflaredManager::saveBindings()
{
    QJsonArray arr;
    for (const auto& b : m_bindings)
        arr.append(b.toJson());
    m_settings->set("CloudflaredBindings",
                    QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

void CloudflaredManager::addBinding(const CloudflaredBinding& b)
{
    m_bindings.append(b);
    saveBindings();
    emit bindingsChanged();
}

void CloudflaredManager::removeBinding(const QString& id)
{
    stopBinding(id);
    m_bindings.removeIf([&id](const CloudflaredBinding& b) { return b.id == id; });
    m_processes.remove(id);
    saveBindings();
    emit bindingsChanged();
}

void CloudflaredManager::updateBinding(const CloudflaredBinding& updated)
{
    for (auto& b : m_bindings) {
        if (b.id == updated.id) {
            b = updated;
            break;
        }
    }
    saveBindings();
    emit bindingsChanged();
}

int CloudflaredManager::bindingIndex(const QString& id) const
{
    for (int i = 0; i < m_bindings.size(); ++i) {
        if (m_bindings[i].id == id)
            return i;
    }
    return -1;
}

QString CloudflaredManager::binaryPath() const
{
    return m_settings->get("CloudflaredBinaryPath").toString();
}

void CloudflaredManager::startBinding(const QString& id)
{
    const int idx = bindingIndex(id);
    if (idx < 0)
        return;

    const auto& b = m_bindings[idx];
    auto& entry = m_processes[id];

    if (entry.process && entry.process->state() != QProcess::NotRunning)
        return;

    const QString bin = binaryPath();
    if (bin.isEmpty()) {
        emit bindingOutput(id,
                           tr("cloudflared binary not found. Please configure it in External Tools "
                              "settings.\n"));
        entry.status = Status::Error;
        emit bindingStatusChanged(id, Status::Error, {});
        return;
    }

    QStringList args;
    if (b.type == CloudflaredBinding::Type::Expose) {
        args << "tunnel"
             << "--url" << QString("tcp://localhost:%1").arg(b.localPort);
    } else {
        args << "access"
             << "tcp"
             << "--hostname" << b.hostname
             << "--url" << QString("localhost:%1").arg(b.localPort);
    }

    auto* process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);

    entry.process = process;
    entry.status = Status::Starting;
    entry.tunnelUrl.clear();

    connect(process, &QProcess::readyReadStandardOutput, this, [this, id, process]() {
        const QString text = QString::fromUtf8(process->readAllStandardOutput());
        emit bindingOutput(id, text);

        auto& e = m_processes[id];
        if (e.tunnelUrl.isEmpty()) {
            static const QRegularExpression urlRe(
                R"((https?|tcp)://[a-zA-Z0-9\-]+\.trycloudflare\.com)",
                QRegularExpression::CaseInsensitiveOption);
            const auto match = urlRe.match(text);
            if (match.hasMatch()) {
                e.tunnelUrl = match.captured(0);
                e.status = Status::Running;
                emit bindingStatusChanged(id, Status::Running, e.tunnelUrl);
            }
        }
        // Mark as running once we see cloudflared's "ready" message
        if (e.status == Status::Starting &&
            (text.contains("Registered tunnel connection") ||
             text.contains("Connection") ||
             text.contains("INF Connected"))) {
            e.status = Status::Running;
            emit bindingStatusChanged(id, Status::Running, e.tunnelUrl);
        }
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, id](int exitCode, QProcess::ExitStatus) {
                auto& e = m_processes[id];
                e.status = Status::Stopped;
                emit bindingOutput(id, tr("Process exited with code %1\n").arg(exitCode));
                emit bindingStatusChanged(id, Status::Stopped, {});
            });

    emit bindingOutput(id, tr("Starting: %1 %2\n").arg(bin, args.join(' ')));
    process->start(bin, args);

    if (!process->waitForStarted(5000)) {
        entry.status = Status::Error;
        emit bindingOutput(id, tr("Failed to start cloudflared process.\n"));
        emit bindingStatusChanged(id, Status::Error, {});
        return;
    }

    // For Access bindings the local listener is available as soon as the process starts
    if (b.type == CloudflaredBinding::Type::Access) {
        entry.status = Status::Running;
        entry.tunnelUrl = QString("localhost:%1").arg(b.localPort);
        emit bindingStatusChanged(id, Status::Running, entry.tunnelUrl);
    }
}

void CloudflaredManager::stopBinding(const QString& id)
{
    if (!m_processes.contains(id))
        return;

    auto& entry = m_processes[id];
    if (entry.process && entry.process->state() != QProcess::NotRunning) {
        entry.process->terminate();
        if (!entry.process->waitForFinished(3000))
            entry.process->kill();
    }
    entry.status = Status::Stopped;
    entry.tunnelUrl.clear();
}

void CloudflaredManager::stopAll()
{
    for (const auto& id : m_processes.keys())
        stopBinding(id);
}

CloudflaredManager::Status CloudflaredManager::statusOf(const QString& id) const
{
    return m_processes.contains(id) ? m_processes[id].status : Status::Stopped;
}

QString CloudflaredManager::tunnelUrlOf(const QString& id) const
{
    return m_processes.contains(id) ? m_processes[id].tunnelUrl : QString{};
}
