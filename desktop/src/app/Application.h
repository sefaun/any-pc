#pragma once

#include <QObject>
#include <QTimer>

#include <memory>

namespace anypc {

class ApiClient;
class AppSettings;
class MainWindow;
class SessionManager;
class SignalingClient;

/**
 * Composition root. Wires services together and drives the device
 * authentication lifecycle (register -> token -> signaling -> refresh).
 */
class Application final : public QObject
{
    Q_OBJECT

public:
    explicit Application(QObject *parent = nullptr);
    ~Application() override;

    void start();

private:
    void authenticate();
    void onAuthenticated(const QString &deviceId, const QString &accessToken, int expiresInSeconds);
    void scheduleRetry();
    void onServerUrlChanged();

    AppSettings *m_settings;
    ApiClient *m_api;
    SignalingClient *m_signaling;
    SessionManager *m_sessions;
    std::unique_ptr<MainWindow> m_window;

    QTimer m_refreshTimer;
    QTimer m_retryTimer;
    int m_retryAttempt = 0;
    bool m_authInFlight = false;
};

} // namespace anypc
