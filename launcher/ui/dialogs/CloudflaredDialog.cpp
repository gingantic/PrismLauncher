// SPDX-License-Identifier: GPL-3.0-only
#include "CloudflaredDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidgetItem>
#include <QTcpServer>
#include <QVBoxLayout>

#include "Application.h"
#include "cloudflared/CloudflaredBinding.h"
#include "cloudflared/CloudflaredManager.h"
#include "ui_CloudflaredDialog.h"

namespace {

QString statusText(CloudflaredManager::Status s)
{
    switch (s) {
        case CloudflaredManager::Status::Stopped:  return QObject::tr("Stopped");
        case CloudflaredManager::Status::Starting: return QObject::tr("Starting…");
        case CloudflaredManager::Status::Running:  return QObject::tr("Running");
        case CloudflaredManager::Status::Error:    return QObject::tr("Error");
    }
    return {};
}

QColor statusColor(CloudflaredManager::Status s)
{
    switch (s) {
        case CloudflaredManager::Status::Running:  return QColor(0x22, 0x8b, 0x22);
        case CloudflaredManager::Status::Starting: return QColor(0xff, 0xa5, 0x00);
        case CloudflaredManager::Status::Error:    return QColor(0xcc, 0x00, 0x00);
        default:                                   return {};
    }
}

}  // namespace

CloudflaredDialog::CloudflaredDialog(QWidget* parent)
    : QDialog(parent), ui(new Ui::CloudflaredDialog)
{
    ui->setupUi(this);

    m_manager = APPLICATION->cloudflaredManager();

    connect(m_manager, &CloudflaredManager::bindingsChanged, this, &CloudflaredDialog::onBindingsChanged);
    connect(m_manager, &CloudflaredManager::bindingStatusChanged, this,
            &CloudflaredDialog::onBindingStatusChanged);
    connect(m_manager, &CloudflaredManager::bindingOutput, this, &CloudflaredDialog::onBindingOutput);

    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    ui->bindingTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->bindingTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->bindingTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    ui->bindingTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->bindingTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->bindingTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    ui->bindingTable->verticalHeader()->setVisible(false);

    refreshTable();
}

CloudflaredDialog::~CloudflaredDialog()
{
    // Disconnect our slots from the shared manager so stale UI callbacks don't fire
    disconnect(m_manager, nullptr, this, nullptr);
    delete ui;
}

void CloudflaredDialog::refreshTable()
{
    const auto& bindings = m_manager->bindings();
    ui->bindingTable->setRowCount(bindings.size());

    for (int row = 0; row < bindings.size(); ++row) {
        const auto& b = bindings[row];
        const auto status = m_manager->statusOf(b.id);
        const auto url = m_manager->tunnelUrlOf(b.id);

        auto* nameItem = new QTableWidgetItem(b.name);
        nameItem->setData(Qt::UserRole, b.id);
        ui->bindingTable->setItem(row, 0, nameItem);

        auto* typeItem = new QTableWidgetItem(
            b.type == CloudflaredBinding::Type::Expose ? tr("Expose") : tr("Access"));
        ui->bindingTable->setItem(row, 1, typeItem);

        auto* hostItem = new QTableWidgetItem(
            b.type == CloudflaredBinding::Type::Expose ? tr("(quick tunnel)") : b.hostname);
        ui->bindingTable->setItem(row, 2, hostItem);

        auto* portItem = new QTableWidgetItem(QString::number(b.localPort));
        portItem->setTextAlignment(Qt::AlignCenter);
        ui->bindingTable->setItem(row, 3, portItem);

        auto* statusItem = new QTableWidgetItem(statusText(status));
        const QColor col = statusColor(status);
        if (col.isValid())
            statusItem->setForeground(col);
        ui->bindingTable->setItem(row, 4, statusItem);

        ui->bindingTable->setItem(row, 5, new QTableWidgetItem(url));
    }

    updateButtonStates();
}

void CloudflaredDialog::updateButtonStates()
{
    const QString id = currentBindingId();
    const bool hasSelection = !id.isEmpty();

    ui->editButton->setEnabled(hasSelection);
    ui->removeButton->setEnabled(hasSelection);
    ui->startStopButton->setEnabled(hasSelection);
    ui->copyUrlButton->setEnabled(hasSelection && !m_manager->tunnelUrlOf(id).isEmpty());

    if (hasSelection) {
        const auto status = m_manager->statusOf(id);
        const bool running = (status == CloudflaredManager::Status::Running ||
                              status == CloudflaredManager::Status::Starting);
        ui->startStopButton->setText(running ? tr("Stop") : tr("Start"));
    } else {
        ui->startStopButton->setText(tr("Start"));
    }
}

QString CloudflaredDialog::currentBindingId() const
{
    const int row = ui->bindingTable->currentRow();
    if (row < 0)
        return {};
    const auto* item = ui->bindingTable->item(row, 0);
    return item ? item->data(Qt::UserRole).toString() : QString{};
}

// Returns true if it is safe to proceed (no conflict, or user chose to ignore).
// excludeId: the binding being edited — skip it when scanning for duplicates.
bool CloudflaredDialog::checkPortConflicts(const CloudflaredBinding& binding,
                                           const QString& excludeId)
{
    // Check other bindings that share the same local port
    for (const auto& b : m_manager->bindings()) {
        if (b.id == excludeId)
            continue;
        if (b.localPort == binding.localPort) {
            const auto reply = QMessageBox::warning(
                this, tr("Port Conflict"),
                tr("Port %1 is already used by binding \"%2\".\n"
                   "Running two tunnels on the same local port may cause issues.\n\n"
                   "Continue anyway?")
                    .arg(binding.localPort)
                    .arg(b.name),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (reply == QMessageBox::No)
                return false;
            break;
        }
    }

    // For Access bindings cloudflared listens on the local port — check OS-level availability
    if (binding.type == CloudflaredBinding::Type::Access) {
        QTcpServer probe;
        if (!probe.listen(QHostAddress::LocalHost, static_cast<quint16>(binding.localPort))) {
            const auto reply = QMessageBox::warning(
                this, tr("Port Already in Use"),
                tr("Port %1 is already bound by another application on this machine.\n"
                   "The tunnel may fail to start.\n\n"
                   "Continue anyway?")
                    .arg(binding.localPort),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (reply == QMessageBox::No)
                return false;
        }
        // probe is destroyed here, releasing the temporary bind
    }

    return true;
}

bool CloudflaredDialog::showBindingEditor(CloudflaredBinding& binding, bool isNew)
{
    QDialog dlg(this);
    dlg.setWindowTitle(isNew ? tr("Add Tunnel Binding") : tr("Edit Tunnel Binding"));
    dlg.setMinimumWidth(420);

    auto* layout = new QVBoxLayout(&dlg);

    auto* form = new QFormLayout;
    layout->addLayout(form);

    auto* nameEdit = new QLineEdit(binding.name, &dlg);
    nameEdit->setPlaceholderText(tr("e.g. My MC Server"));
    form->addRow(tr("Name:"), nameEdit);

    auto* typeCombo = new QComboBox(&dlg);
    typeCombo->addItem(tr("Expose  (share local port via Cloudflare URL)"),
                       QVariant::fromValue(int(CloudflaredBinding::Type::Expose)));
    typeCombo->addItem(tr("Access  (connect to remote Cloudflare hostname locally)"),
                       QVariant::fromValue(int(CloudflaredBinding::Type::Access)));
    typeCombo->setCurrentIndex(binding.type == CloudflaredBinding::Type::Access ? 1 : 0);
    form->addRow(tr("Type:"), typeCombo);

    auto* hostnameEdit = new QLineEdit(binding.hostname, &dlg);
    hostnameEdit->setPlaceholderText(tr("e.g. abc.trycloudflare.com"));
    auto* hostnameLabel = new QLabel(tr("Hostname:"), &dlg);
    form->addRow(hostnameLabel, hostnameEdit);

    auto* portSpin = new QSpinBox(&dlg);
    portSpin->setRange(1, 65535);
    portSpin->setValue(binding.localPort);
    form->addRow(tr("Local Port:"), portSpin);

    auto* autoStartCheck = new QCheckBox(tr("Start automatically when an instance launches"), &dlg);
    autoStartCheck->setChecked(binding.autoStart);
    layout->addWidget(autoStartCheck);

    // Show/hide hostname based on type
    auto updateHostnameVisibility = [&]() {
        const bool isAccess = typeCombo->currentData().toInt() ==
                              int(CloudflaredBinding::Type::Access);
        hostnameLabel->setVisible(isAccess);
        hostnameEdit->setVisible(isAccess);
    };
    updateHostnameVisibility();
    connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [updateHostnameVisibility](int) { updateHostnameVisibility(); });

    auto* infoLabel = new QLabel(
        tr("<i>Expose</i>: cloudflared creates a public TCP tunnel URL for <tt>localhost:PORT</tt>.<br>"
           "<i>Access</i>: cloudflared forwards <tt>HOSTNAME</tt> → <tt>localhost:PORT</tt> "
           "so Minecraft can connect to localhost."),
        &dlg);
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dlg);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return false;

    binding.name = nameEdit->text().trimmed();
    if (binding.name.isEmpty())
        binding.name = tr("Unnamed");
    binding.type = static_cast<CloudflaredBinding::Type>(typeCombo->currentData().toInt());
    binding.hostname = hostnameEdit->text().trimmed();
    binding.localPort = portSpin->value();
    binding.autoStart = autoStartCheck->isChecked();
    return true;
}

void CloudflaredDialog::on_addButton_clicked()
{
    auto binding = CloudflaredBinding::create();
    binding.name = tr("New Binding");
    if (!showBindingEditor(binding, true))
        return;
    if (!checkPortConflicts(binding, {}))
        return;
    m_manager->addBinding(binding);
}

void CloudflaredDialog::on_editButton_clicked()
{
    const QString id = currentBindingId();
    if (id.isEmpty())
        return;

    const auto& bindings = m_manager->bindings();
    const auto it = std::find_if(bindings.begin(), bindings.end(),
                                 [&id](const CloudflaredBinding& b) { return b.id == id; });
    if (it == bindings.end())
        return;

    const int oldPort = it->localPort;
    CloudflaredBinding copy = *it;

    if (!showBindingEditor(copy, false))
        return;

    // Only re-check port conflicts when the port actually changed
    if (copy.localPort != oldPort && !checkPortConflicts(copy, id))
        return;

    const bool wasRunning = (m_manager->statusOf(id) == CloudflaredManager::Status::Running ||
                             m_manager->statusOf(id) == CloudflaredManager::Status::Starting);

    m_manager->updateBinding(copy);

    if (wasRunning) {
        ui->logOutput->appendPlainText(
            tr("[%1] Configuration changed — restarting tunnel…").arg(copy.name));
        m_manager->stopBinding(id);
        m_manager->startBinding(id);
    }
}

void CloudflaredDialog::on_removeButton_clicked()
{
    const QString id = currentBindingId();
    if (id.isEmpty())
        return;

    const auto reply = QMessageBox::question(
        this, tr("Remove Binding"),
        tr("Remove this tunnel binding? Any running tunnel will be stopped."),
        QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes)
        m_manager->removeBinding(id);
}

void CloudflaredDialog::on_startStopButton_clicked()
{
    const QString id = currentBindingId();
    if (id.isEmpty())
        return;

    const auto status = m_manager->statusOf(id);
    if (status == CloudflaredManager::Status::Running ||
        status == CloudflaredManager::Status::Starting) {
        m_manager->stopBinding(id);
        ui->logOutput->appendPlainText(tr("[%1] Stopped.\n").arg(id.left(8)));
    } else {
        ui->logOutput->appendPlainText(tr("[%1] Starting…\n").arg(id.left(8)));
        m_manager->startBinding(id);
    }
}

void CloudflaredDialog::on_copyUrlButton_clicked()
{
    const QString url = m_manager->tunnelUrlOf(currentBindingId());
    if (!url.isEmpty())
        QApplication::clipboard()->setText(url);
}

void CloudflaredDialog::on_bindingTable_currentCellChanged(
    int /*currentRow*/, int /*currentColumn*/,
    int /*previousRow*/, int /*previousColumn*/)
{
    updateButtonStates();
}

void CloudflaredDialog::onBindingsChanged()
{
    refreshTable();
}

void CloudflaredDialog::onBindingStatusChanged(const QString& id, CloudflaredManager::Status status,
                                               const QString& tunnelUrl)
{
    const auto& bindings = m_manager->bindings();
    for (int row = 0; row < bindings.size(); ++row) {
        if (bindings[row].id != id)
            continue;

        const auto* nameItem = ui->bindingTable->item(row, 0);
        if (!nameItem)
            break;

        auto* statusItem = ui->bindingTable->item(row, 4);
        if (statusItem) {
            statusItem->setText(statusText(status));
            const QColor col = statusColor(status);
            if (col.isValid())
                statusItem->setForeground(col);
            else
                statusItem->setForeground(ui->bindingTable->palette().text());
        }

        auto* urlItem = ui->bindingTable->item(row, 5);
        if (urlItem)
            urlItem->setText(tunnelUrl);
        break;
    }

    updateButtonStates();

    if (status == CloudflaredManager::Status::Running && !tunnelUrl.isEmpty()) {
        ui->logOutput->appendPlainText(tr("Tunnel URL: %1").arg(tunnelUrl));
    }
}

void CloudflaredDialog::onBindingOutput(const QString& id, const QString& text)
{
    // Prefix each line with the binding name for readability
    const auto& bindings = m_manager->bindings();
    QString prefix;
    for (const auto& b : bindings) {
        if (b.id == id) {
            prefix = QStringLiteral("[%1] ").arg(b.name);
            break;
        }
    }

    for (const auto& line : text.split('\n', Qt::SkipEmptyParts))
        ui->logOutput->appendPlainText(prefix + line);
}
