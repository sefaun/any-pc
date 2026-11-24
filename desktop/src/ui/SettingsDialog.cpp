#include "ui/SettingsDialog.h"

#include "core/AppSettings.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QVBoxLayout>

namespace anypc {

namespace {
constexpr int kMinPasswordLength = 6;
} // namespace

SettingsDialog::SettingsDialog(AppSettings *settings, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
    , m_serverUrl(new QLineEdit(settings->serverUrl().toString(), this))
    , m_deviceName(new QLineEdit(settings->deviceName(), this))
    , m_password(new QLineEdit(this))
    , m_passwordConfirm(new QLineEdit(this))
    , m_passwordState(new QLabel(this))
    , m_clearPassword(new QCheckBox(tr("Disable unattended access"), this))
    , m_clipboardSync(new QCheckBox(tr("Synchronise clipboard text during sessions"), this))
    , m_minimizeToTray(new QCheckBox(tr("Keep running in the system tray when the window is closed"), this))
{
    setWindowTitle(tr("Settings"));
    setMinimumWidth(480);

    m_serverUrl->setPlaceholderText(QStringLiteral("https://anypc.example.com"));
    m_deviceName->setMaxLength(64);
    m_password->setEchoMode(QLineEdit::Password);
    m_passwordConfirm->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText(tr("Leave empty to keep the current password"));
    m_clipboardSync->setChecked(settings->clipboardSyncEnabled());
    m_minimizeToTray->setChecked(settings->minimizeToTray());

    auto *general = new QGroupBox(tr("General"), this);
    auto *generalForm = new QFormLayout(general);
    generalForm->addRow(tr("Server URL"), m_serverUrl);
    generalForm->addRow(tr("Device name"), m_deviceName);
    generalForm->addRow(QString(), m_clipboardSync);
    generalForm->addRow(QString(), m_minimizeToTray);

    auto *security = new QGroupBox(tr("Unattended access"), this);
    auto *securityForm = new QFormLayout(security);
    auto *hint = new QLabel(tr("With a password set, remote users who know it can connect without "
                               "anyone accepting the request on this computer."), this);
    hint->setWordWrap(true);
    hint->setObjectName(QStringLiteral("cardHint"));
    securityForm->addRow(hint);
    securityForm->addRow(tr("Status"), m_passwordState);
    securityForm->addRow(tr("New password"), m_password);
    securityForm->addRow(tr("Confirm"), m_passwordConfirm);
    securityForm->addRow(QString(), m_clearPassword);
    connect(m_clearPassword, &QCheckBox::toggled, this, [this](bool on) {
        m_password->setEnabled(!on);
        m_passwordConfirm->setEnabled(!on);
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(general);
    layout->addWidget(security);
    layout->addWidget(buttons);
    updatePasswordState();
}

void SettingsDialog::updatePasswordState()
{
    const bool enabled = m_settings->hasUnattendedPassword();
    m_passwordState->setText(enabled ? tr("Enabled") : tr("Disabled"));
    m_clearPassword->setVisible(enabled);
}

void SettingsDialog::accept()
{
    const QUrl url = QUrl::fromUserInput(m_serverUrl->text().trimmed());
    if (!url.isValid() || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))) {
        QMessageBox::warning(this, windowTitle(), tr("Please enter a valid http(s) server URL."));
        return;
    }
    if (m_deviceName->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, windowTitle(), tr("The device name cannot be empty."));
        return;
    }

    const QString password = m_password->text();
    if (!m_clearPassword->isChecked() && !password.isEmpty()) {
        if (password.size() < kMinPasswordLength) {
            QMessageBox::warning(this, windowTitle(),
                                 tr("The password must be at least %1 characters long.").arg(kMinPasswordLength));
            return;
        }
        if (password != m_passwordConfirm->text()) {
            QMessageBox::warning(this, windowTitle(), tr("The passwords do not match."));
            return;
        }
    }

    m_settings->setDeviceName(m_deviceName->text());
    m_settings->setClipboardSyncEnabled(m_clipboardSync->isChecked());
    m_settings->setMinimizeToTray(m_minimizeToTray->isChecked());
    if (m_clearPassword->isChecked())
        m_settings->clearUnattendedPassword();
    else if (!password.isEmpty())
        m_settings->setUnattendedPassword(password);
    m_settings->setServerUrl(url.adjusted(QUrl::StripTrailingSlash));
    QDialog::accept();
}

} // namespace anypc
