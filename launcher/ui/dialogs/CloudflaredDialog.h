// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QDialog>

#include "cloudflared/CloudflaredManager.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class CloudflaredDialog;
}
QT_END_NAMESPACE

struct CloudflaredBinding;

class CloudflaredDialog : public QDialog {
    Q_OBJECT

   public:
    explicit CloudflaredDialog(QWidget* parent = nullptr);
    ~CloudflaredDialog() override;

   private slots:
    void on_addButton_clicked();
    void on_editButton_clicked();
    void on_removeButton_clicked();
    void on_startStopButton_clicked();
    void on_copyUrlButton_clicked();
    void on_bindingTable_currentCellChanged(int currentRow, int currentColumn, int previousRow, int previousColumn);

    void onBindingsChanged();
    void onBindingStatusChanged(const QString& id, CloudflaredManager::Status status, const QString& tunnelUrl);
    void onBindingOutput(const QString& id, const QString& text);

   private:
    void refreshTable();
    void updateButtonStates();
    QString currentBindingId() const;
    bool showBindingEditor(CloudflaredBinding& binding, bool isNew);
    bool checkPortConflicts(const CloudflaredBinding& binding, const QString& excludeId);

    Ui::CloudflaredDialog* ui;
    CloudflaredManager* m_manager;
};
