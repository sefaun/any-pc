#pragma once

#include <QDialog>
#include <QUrl>

class QCheckBox;
class QLineEdit;
class QLabel;

namespace anypc {

class AppSettings;

/** Edits server, device and security settings. */
class SettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(AppSettings *settings, QWidget *parent = nullptr);

    void accept() override;

private:
    void updatePasswordState();

    AppSettings *m_settings;
    QLineEdit *m_serverUrl;
    QUrl m_pendingServerUrl;
    QLineEdit *m_deviceName;
    QLineEdit *m_password;
    QLineEdit *m_passwordConfirm;
    QLabel *m_passwordState;
    QCheckBox *m_clearPassword;
    QCheckBox *m_clipboardSync;
    QCheckBox *m_minimizeToTray;
};

} // namespace anypc
