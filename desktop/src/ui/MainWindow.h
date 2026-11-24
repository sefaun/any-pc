#pragma once

#include "network/Protocol.h"

#include <QHash>
#include <QMainWindow>
#include <QPointer>

class QAction;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QSystemTrayIcon;

namespace anypc {

class AppSettings;
class IncomingRequestDialog;
class SessionManager;

/** Main window: shows this device's address and starts outgoing connections. */
class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    enum class ServerState { Offline, Connecting, Online };

    MainWindow(AppSettings *settings, SessionManager *sessions, QWidget *parent = nullptr);

    void setDeviceId(const QString &deviceId);
    void setServerState(ServerState state, const QString &detail = {});
    void showMessage(const QString &message);
    void bringToFront();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QWidget *buildLocalCard();
    QWidget *buildRemoteCard();
    QFrame *buildControlBanner();
    void setupMenus();
    void setupTray();
    void refreshUnattendedLabel();
    void refreshRecent();
    void startConnection();
    void setOutgoingBusy(bool busy, const QString &text = {});
    void showIncomingRequest(const QString &connectionId, const protocol::DeviceSummary &from, int expiresInMs);
    void openSettings();
    void showAbout();
    void quit();

    AppSettings *m_settings;
    SessionManager *m_sessions;

    QLabel *m_deviceId = nullptr;
    QLabel *m_unattended = nullptr;
    QLineEdit *m_remoteId = nullptr;
    QLineEdit *m_remotePassword = nullptr;
    QPushButton *m_connectButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QLabel *m_outgoingStatus = nullptr;
    QFrame *m_controlBanner = nullptr;
    QLabel *m_controlLabel = nullptr;
    QLabel *m_statusDot = nullptr;
    QLabel *m_statusText = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QString m_rawDeviceId;
    QHash<QString, QPointer<IncomingRequestDialog>> m_incomingDialogs;
    bool m_quitting = false;
};

} // namespace anypc
