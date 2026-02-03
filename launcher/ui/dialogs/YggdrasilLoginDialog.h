// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QtWidgets/QDialog>

#include "minecraft/auth/MinecraftAccount.h"

namespace Ui {
class YggdrasilLoginDialog;
}

class YggdrasilLoginDialog : public QDialog {
    Q_OBJECT

   public:
    ~YggdrasilLoginDialog();

    static MinecraftAccountPtr newAccount(QWidget* parent);

   private:
    explicit YggdrasilLoginDialog(QWidget* parent = 0);

   private slots:
    void startLogin();

   private:
    Ui::YggdrasilLoginDialog* ui;
    MinecraftAccountPtr m_account;
};
