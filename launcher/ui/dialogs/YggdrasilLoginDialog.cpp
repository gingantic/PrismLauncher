// SPDX-License-Identifier: GPL-3.0-only
#include "YggdrasilLoginDialog.h"
#include "ui_YggdrasilLoginDialog.h"

#include <QPushButton>
#include <QUrl>

#include "ui/dialogs/ProgressDialog.h"
#include "tasks/Task.h"

YggdrasilLoginDialog::YggdrasilLoginDialog(QWidget* parent) : QDialog(parent), ui(new Ui::YggdrasilLoginDialog)
{
    ui->setupUi(this);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Add"));
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));

    connect(ui->buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &YggdrasilLoginDialog::startLogin);
    connect(ui->buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &YggdrasilLoginDialog::reject);
}

YggdrasilLoginDialog::~YggdrasilLoginDialog()
{
    delete ui;
}

MinecraftAccountPtr YggdrasilLoginDialog::newAccount(QWidget* parent)
{
    YggdrasilLoginDialog dlg(parent);
    if (dlg.exec() == QDialog::Accepted) {
        return dlg.m_account;
    }
    return nullptr;
}

void YggdrasilLoginDialog::startLogin()
{
    ui->statusLabel->clear();

    const auto serverUrlText = ui->authServerUrl->text().trimmed();
    const auto username = ui->username->text().trimmed();
    const auto password = ui->password->text();

    if (serverUrlText.isEmpty()) {
        ui->statusLabel->setText(tr("<font color='red'>Authlib-injector base URL is required.</font>"));
        return;
    }
    if (username.isEmpty() || password.isEmpty()) {
        ui->statusLabel->setText(tr("<font color='red'>Username and password are required.</font>"));
        return;
    }

    QUrl serverUrl = QUrl::fromUserInput(serverUrlText);
    if (!serverUrl.isValid() || serverUrl.scheme().isEmpty()) {
        ui->statusLabel->setText(tr("<font color='red'>Authlib-injector base URL is invalid.</font>"));
        return;
    }

    m_account = MinecraftAccount::createBlankYggdrasil();
    auto data = m_account->accountData();
    data->yggdrasilServerUrl = serverUrl.toString();
    data->yggdrasilUserName = username;
    data->yggdrasilPassword = password;

    auto task = m_account->login();
    ProgressDialog progDialog(this);
    progDialog.setSkipButton(true, tr("Abort"));
    progDialog.execWithTask(task.get());

    if (task->getState() == Task::State::Succeeded) {
        accept();
        return;
    }

    QString error = task->failReason();
    if (error.isEmpty()) {
        error = tr("Authentication failed.");
    }
    ui->statusLabel->setText(tr("<font color='red'>%1</font>").arg(error));
    m_account.reset();
}
