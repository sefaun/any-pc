#pragma once

#include <QDialog>
#include <QPointer>
#include <QUrl>

class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QNetworkReply;
class QPushButton;

namespace anypc {

/**
 * Asks for the AnyPC server address and verifies it by calling the backend
 * health endpoint. Shown on first launch, so the server does not have to be
 * baked into the build.
 */
class ServerSetupDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ServerSetupDialog(const QUrl &initial, QWidget *parent = nullptr);

    /** The confirmed server URL; valid only after the dialog was accepted. */
    QUrl serverUrl() const { return m_serverUrl; }

private:
    void verify();
    void onReplyFinished();
    void setBusy(bool busy);
    void showError(const QString &message, bool allowSaveAnyway);

    QNetworkAccessManager *m_network;
    QPointer<QNetworkReply> m_reply;
    QLineEdit *m_address;
    QLabel *m_status;
    QPushButton *m_connectButton;
    QPushButton *m_saveAnywayButton;
    QUrl m_candidate;
    QUrl m_serverUrl;
};

} // namespace anypc
