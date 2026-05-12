// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "ExternalToolsPage.h"
#include "ui_ExternalToolsPage.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTabBar>

#include <FileSystem.h>
#include <tasks/Task.h>
#include <tools/MCEditTool.h>
#include "Application.h"
#include "authlib/AuthlibInjectorUpdateTask.h"
#include "cloudflared/CloudflaredUpdateTask.h"
#include "settings/SettingsObject.h"
#include "tools/BaseProfiler.h"
#include "ui/dialogs/CloudflaredDialog.h"
#include "ui/dialogs/ProgressDialog.h"

ExternalToolsPage::ExternalToolsPage(QWidget* parent) : QWidget(parent), ui(new Ui::ExternalToolsPage)
{
    ui->setupUi(this);

    ui->jsonEditorTextBox->setClearButtonEnabled(true);

    ui->mceditLink->setOpenExternalLinks(true);
    ui->jvisualvmLink->setOpenExternalLinks(true);
    ui->jprofilerLink->setOpenExternalLinks(true);
    loadSettings();
}

ExternalToolsPage::~ExternalToolsPage()
{
    delete ui;
}

void ExternalToolsPage::loadSettings()
{
    auto s = APPLICATION->settings();
    ui->jprofilerPathEdit->setText(s->get("JProfilerPath").toString());
    ui->jvisualvmPathEdit->setText(s->get("JVisualVMPath").toString());
    ui->mceditPathEdit->setText(s->get("MCEditPath").toString());

    // Editors
    ui->jsonEditorTextBox->setText(s->get("JsonEditor").toString());
    ui->authlibInjectorAutoUpdate->setChecked(s->get("AuthlibInjectorAutoUpdate").toBool());
    updateAuthlibInjectorStatus();
    ui->cloudflaredAutoUpdate->setChecked(s->get("CloudflaredAutoUpdate").toBool());
    updateCloudflaredStatus();
}
void ExternalToolsPage::applySettings()
{
    auto s = APPLICATION->settings();

    s->set("JProfilerPath", ui->jprofilerPathEdit->text());
    s->set("JVisualVMPath", ui->jvisualvmPathEdit->text());
    s->set("MCEditPath", ui->mceditPathEdit->text());

    // Editors
    QString jsonEditor = ui->jsonEditorTextBox->text();
    if (!jsonEditor.isEmpty() && (!QFileInfo(jsonEditor).exists() || !QFileInfo(jsonEditor).isExecutable())) {
        QString found = QStandardPaths::findExecutable(jsonEditor);
        if (!found.isEmpty()) {
            jsonEditor = found;
        }
    }
    s->set("JsonEditor", jsonEditor);
    s->set("AuthlibInjectorAutoUpdate", ui->authlibInjectorAutoUpdate->isChecked());
    s->set("CloudflaredAutoUpdate", ui->cloudflaredAutoUpdate->isChecked());
}

void ExternalToolsPage::updateAuthlibInjectorStatus()
{
    auto s = APPLICATION->settings();
    const auto tag = s->get("AuthlibInjectorLatestTag").toString();
    const auto jarPath = s->get("AuthlibInjectorJarPath").toString();
    if (!jarPath.isEmpty() && QFileInfo(jarPath).isFile()) {
        if (!tag.isEmpty()) {
            ui->authlibInjectorStatus->setText(tr("Installed: %1").arg(tag));
        } else {
            ui->authlibInjectorStatus->setText(tr("Installed"));
        }
    } else {
        ui->authlibInjectorStatus->setText(tr("Not downloaded"));
    }
}

void ExternalToolsPage::updateCloudflaredStatus()
{
    auto s = APPLICATION->settings();
    const auto tag = s->get("CloudflaredLatestTag").toString();
    const auto binaryPath = s->get("CloudflaredBinaryPath").toString();
    if (!binaryPath.isEmpty() && QFileInfo(binaryPath).isFile()) {
        if (!tag.isEmpty()) {
            ui->cloudflaredStatus->setText(tr("Installed: %1").arg(tag));
        } else {
            ui->cloudflaredStatus->setText(tr("Installed"));
        }
    } else {
        ui->cloudflaredStatus->setText(tr("Not downloaded"));
    }
}

void ExternalToolsPage::on_jprofilerPathBtn_clicked()
{
    QString raw_dir = ui->jprofilerPathEdit->text();
    QString error;
    do {
        raw_dir = QFileDialog::getExistingDirectory(this, tr("JProfiler Folder"), raw_dir);
        if (raw_dir.isEmpty()) {
            break;
        }
        QString cooked_dir = FS::NormalizePath(raw_dir);
        if (!APPLICATION->profilers()["jprofiler"]->check(cooked_dir, &error)) {
            QMessageBox::critical(this, tr("Error"), tr("Error while checking JProfiler install:\n%1").arg(error));
            continue;
        } else {
            ui->jprofilerPathEdit->setText(cooked_dir);
            break;
        }
    } while (1);
}
void ExternalToolsPage::on_jprofilerCheckBtn_clicked()
{
    QString error;
    if (!APPLICATION->profilers()["jprofiler"]->check(ui->jprofilerPathEdit->text(), &error)) {
        QMessageBox::critical(this, tr("Error"), tr("Error while checking JProfiler install:\n%1").arg(error));
    } else {
        QMessageBox::information(this, tr("OK"), tr("JProfiler setup seems to be OK"));
    }
}

void ExternalToolsPage::on_jvisualvmPathBtn_clicked()
{
    QString raw_dir = ui->jvisualvmPathEdit->text();
    QString error;
    do {
        raw_dir = QFileDialog::getOpenFileName(this, tr("VisualVM Executable"), raw_dir);
        if (raw_dir.isEmpty()) {
            break;
        }
        QString cooked_dir = FS::NormalizePath(raw_dir);
        if (!APPLICATION->profilers()["jvisualvm"]->check(cooked_dir, &error)) {
            QMessageBox::critical(this, tr("Error"), tr("Error while checking VisualVM install:\n%1").arg(error));
            continue;
        } else {
            ui->jvisualvmPathEdit->setText(cooked_dir);
            break;
        }
    } while (1);
}
void ExternalToolsPage::on_jvisualvmCheckBtn_clicked()
{
    QString error;
    if (!APPLICATION->profilers()["jvisualvm"]->check(ui->jvisualvmPathEdit->text(), &error)) {
        QMessageBox::critical(this, tr("Error"), tr("Error while checking VisualVM install:\n%1").arg(error));
    } else {
        QMessageBox::information(this, tr("OK"), tr("VisualVM setup seems to be OK"));
    }
}

void ExternalToolsPage::on_mceditPathBtn_clicked()
{
    QString raw_dir = ui->mceditPathEdit->text();
    QString error;
    do {
#ifdef Q_OS_MACOS
        raw_dir = QFileDialog::getOpenFileName(this, tr("MCEdit Application"), raw_dir);
#else
        raw_dir = QFileDialog::getExistingDirectory(this, tr("MCEdit Folder"), raw_dir);
#endif
        if (raw_dir.isEmpty()) {
            break;
        }
        QString cooked_dir = FS::NormalizePath(raw_dir);
        if (!APPLICATION->mcedit()->check(cooked_dir, error)) {
            QMessageBox::critical(this, tr("Error"), tr("Error while checking MCEdit install:\n%1").arg(error));
            continue;
        } else {
            ui->mceditPathEdit->setText(cooked_dir);
            break;
        }
    } while (1);
}
void ExternalToolsPage::on_mceditCheckBtn_clicked()
{
    QString error;
    if (!APPLICATION->mcedit()->check(ui->mceditPathEdit->text(), error)) {
        QMessageBox::critical(this, tr("Error"), tr("Error while checking MCEdit install:\n%1").arg(error));
    } else {
        QMessageBox::information(this, tr("OK"), tr("MCEdit setup seems to be OK"));
    }
}

void ExternalToolsPage::on_jsonEditorBrowseBtn_clicked()
{
    QString raw_file = QFileDialog::getOpenFileName(this, tr("Text Editor"),
                                                    ui->jsonEditorTextBox->text().isEmpty()
#if defined(Q_OS_LINUX)
                                                        ? QString("/usr/bin")
#else
                                                        ? QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation).first()
#endif
                                                        : ui->jsonEditorTextBox->text());

    if (raw_file.isEmpty()) {
        return;
    }
    QString cooked_file = FS::NormalizePath(raw_file);

    // it has to exist and be an executable
    if (QFileInfo(cooked_file).exists() && QFileInfo(cooked_file).isExecutable()) {
        ui->jsonEditorTextBox->setText(cooked_file);
    } else {
        QMessageBox::warning(this, tr("Invalid"), tr("The file chosen does not seem to be an executable"));
    }
}

void ExternalToolsPage::on_authlibInjectorCheckBtn_clicked()
{
    auto task = makeShared<AuthlibInjectorUpdateTask>(APPLICATION->settings(), APPLICATION->network(), APPLICATION->dataRoot());
    ProgressDialog progDialog(this);
    progDialog.setSkipButton(true, tr("Abort"));
    progDialog.execWithTask(task.get());

    updateAuthlibInjectorStatus();

    if (task->getState() == Task::State::Failed) {
        auto reason = task->failReason();
        if (reason.isEmpty()) {
            reason = tr("Authlib-injector update failed.");
        }
        QMessageBox::warning(this, tr("Error"), reason);
    }
}

void ExternalToolsPage::on_cloudflaredCheckBtn_clicked()
{
    auto task = makeShared<CloudflaredUpdateTask>(APPLICATION->settings(), APPLICATION->network(), APPLICATION->dataRoot());
    ProgressDialog progDialog(this);
    progDialog.setSkipButton(true, tr("Abort"));
    progDialog.execWithTask(task.get());

    updateCloudflaredStatus();

    if (task->getState() == Task::State::Failed) {
        auto reason = task->failReason();
        if (reason.isEmpty()) {
            reason = tr("Cloudflared update failed.");
        }
        QMessageBox::warning(this, tr("Error"), reason);
    }
}

void ExternalToolsPage::on_cloudflaredManageTunnelsBtn_clicked()
{
    CloudflaredDialog dlg(this);
    dlg.exec();
}

bool ExternalToolsPage::apply()
{
    applySettings();
    return true;
}

void ExternalToolsPage::retranslate()
{
    ui->retranslateUi(this);
}
