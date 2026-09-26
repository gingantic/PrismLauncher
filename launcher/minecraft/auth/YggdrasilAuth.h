// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <memory>

#include "minecraft/auth/AuthStep.h"
#include "net/NetJob.h"
#include "net/Request.h"

class YggdrasilLoginStep : public AuthStep {
    Q_OBJECT

   public:
    explicit YggdrasilLoginStep(AccountData* data);
    virtual ~YggdrasilLoginStep() noexcept = default;

    void perform() override;
    QString describe() override;

   private slots:
    void onRequestDone(QByteArray* response);

   private:
    Net::Request::Ptr m_request;
    NetJob::Ptr m_task;
};

class YggdrasilRefreshStep : public AuthStep {
    Q_OBJECT

   public:
    explicit YggdrasilRefreshStep(AccountData* data);
    virtual ~YggdrasilRefreshStep() noexcept = default;

    void perform() override;
    QString describe() override;

   private slots:
    void onRequestDone(QByteArray* response);

   private:
    Net::Request::Ptr m_request;
    NetJob::Ptr m_task;
};

class YggdrasilProfileStep : public AuthStep {
    Q_OBJECT

   public:
    explicit YggdrasilProfileStep(AccountData* data);
    virtual ~YggdrasilProfileStep() noexcept = default;

    void perform() override;
    QString describe() override;

   private slots:
    void onRequestDone(QByteArray* response);

   private:
    Net::Request::Ptr m_request;
    NetJob::Ptr m_task;
};
