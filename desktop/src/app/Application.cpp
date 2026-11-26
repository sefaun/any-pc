#include "app/Application.h"

#include "BuildInfo.h"
#include "core/AppSettings.h"
#include "core/DeviceId.h"
#include "core/ScreenCapturePermission.h"
#include "network/ApiClient.h"
#include "network/SignalingClient.h"
#include "session/SessionManager.h"
#include "ui/MainWindow.h"
#include "ui/ServerSetupDialog.h"

#include <QJsonObject>
#include <QSignalBlocker>
#include <QLoggingCategory>

#include <algorithm>

Q_LOGGING_CATEGORY(lcApp, "anypc.app")

namespace anypc {

namespace {
constexpr int kMaxRetryDelayMs = 30000;

QString platformId()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("linux");
#else
    return QStringLiteral("unknown");
#endif
}
} // namespace

Application::Application(QObject *parent)
    : QObject(parent)
    , m_settings(new AppSettings(this))
    , m_api(new ApiClient(this))
    , m_signaling(new SignalingClient(this))
    , m_sessions(new SessionManager(m_api, m_signaling, m_settings, this))
    , m_window(std::make_unique<MainWindow>(m_settings, m_sessions))
{
    m_refreshTimer.setSingleShot(true);
    m_retryTimer.setSingleShot(true);
    connect(&m_refreshTimer, &QTimer::timeout, this, &Application::authenticate);
    connect(&m_retryTimer, &QTimer::timeout, this, &Application::authenticate);

    connect(m_signaling, &SignalingClient::authenticationFailed, this, &Application::authenticate);
    connect(m_signaling, &SignalingClient::stateChanged, this, [this](SignalingClient::State state) {
        switch (state) {
        case SignalingClient::State::Connected:
            m_window->setServerState(MainWindow::ServerState::Online);
            break;
        case SignalingClient::State::Connecting:
            m_window->setServerState(MainWindow::ServerState::Connecting);
            break;
        case SignalingClient::State::Disconnected:
            m_window->setServerState(MainWindow::ServerState::Offline, tr("Reconnecting…"));
            break;
        }
    });

    connect(m_settings, &AppSettings::serverUrlChanged, this, &Application::onServerUrlChanged);
    connect(m_settings, &AppSettings::deviceNameChanged, this, &Application::authenticate);
    connect(m_window.get(), &MainWindow::changeDeviceIdRequested, this, &Application::changeDeviceId);
}

Application::~Application()
{
    m_signaling->disconnectFromServer();
}

bool Application::start()
{
    if (!m_settings->hasServerUrl()) {
        // First launch: the server is chosen at runtime instead of at build time.
        ServerSetupDialog setup(AppSettings::suggestedServerUrl());
        if (setup.exec() != QDialog::Accepted)
            return false;
        QSignalBlocker blocker(m_settings);
        m_settings->setServerUrl(setup.serverUrl());
    }
    m_api->setBaseUrl(m_settings->serverUrl());
    m_window->show();
    // macOS: ask for Screen Recording access up front (the system prompt is
    // shown once), so the first incoming connection does not fail.
    if (!ScreenCapturePermission::isGranted())
        ScreenCapturePermission::request();
    authenticate();
    return true;
}

void Application::onServerUrlChanged()
{
    qCInfo(lcApp) << "Server URL changed to" << m_settings->serverUrl();
    m_sessions->endAll();
    m_signaling->disconnectFromServer();
    m_api->setBaseUrl(m_settings->serverUrl());
    m_api->setAccessToken({});
    m_retryAttempt = 0;
    authenticate();
}

void Application::changeDeviceId()
{
    m_api->post(QStringLiteral("/devices/me/change-id"), QJsonObject{}, [this](const ApiResponse &response) {
        if (!response.ok()) {
            qCWarning(lcApp) << "Changing the device ID failed:" << response.status << response.errorMessage;
            m_window->showMessage(response.status == 404
                                      ? tr("The server does not support new addresses yet; update it to 1.1.0 or later.")
                                      : tr("Could not get a new address: %1").arg(response.errorMessage));
            return;
        }
        const QJsonObject json = response.body.toObject();
        const QString id = json.value(QLatin1String("deviceId")).toString();
        const QString token = json.value(QLatin1String("accessToken")).toString();
        if (id.isEmpty() || token.isEmpty()) {
            m_window->showMessage(tr("Could not get a new address: unexpected server response."));
            return;
        }
        qCInfo(lcApp) << "Device ID changed to" << id;
        // The secret is unchanged; only the address moves. Reconnect under the new one.
        m_settings->setDeviceCredentials(id, m_settings->deviceSecret());
        m_sessions->endAll();
        m_signaling->disconnectFromServer();
        onAuthenticated(id, token, json.value(QLatin1String("expiresIn")).toInt());
        m_window->showMessage(tr("This computer's new address is %1.").arg(DeviceId::format(id)));
    });
}

void Application::authenticate()
{
    if (m_authInFlight)
        return;
    m_authInFlight = true;
    m_retryTimer.stop();
    m_window->setServerState(MainWindow::ServerState::Connecting);

    QJsonObject body{
        {QStringLiteral("name"), m_settings->deviceName()},
        {QStringLiteral("platform"), platformId()},
        {QStringLiteral("appVersion"), QString::fromLatin1(build::kVersion)},
    };
    const QString deviceId = m_settings->deviceId();
    const QString secret = m_settings->deviceSecret();
    if (!deviceId.isEmpty() && !secret.isEmpty()) {
        body.insert(QStringLiteral("deviceId"), deviceId);
        body.insert(QStringLiteral("deviceSecret"), secret);
    }

    // Registration must not carry a stale token.
    m_api->setAccessToken({});
    m_api->post(QStringLiteral("/devices/register"), body, [this](const ApiResponse &response) {
        m_authInFlight = false;
        if (response.ok()) {
            const QJsonObject json = response.body.toObject();
            const QString id = json.value(QLatin1String("deviceId")).toString();
            const QString newSecret = json.value(QLatin1String("deviceSecret")).toString();
            if (!newSecret.isEmpty())
                m_settings->setDeviceCredentials(id, newSecret);
            onAuthenticated(id, json.value(QLatin1String("accessToken")).toString(),
                            json.value(QLatin1String("expiresIn")).toInt());
            return;
        }
        if (response.status == 401) {
            // Our ID is owned by different credentials; start over with a fresh identity.
            qCWarning(lcApp) << "Stored device credentials were rejected; re-registering";
            m_settings->clearDeviceCredentials();
            authenticate();
            return;
        }
        qCWarning(lcApp) << "Registration failed:" << response.status << response.errorMessage;
        m_window->setServerState(MainWindow::ServerState::Offline, response.errorMessage);
        scheduleRetry();
    });
}

void Application::onAuthenticated(const QString &deviceId, const QString &accessToken, int expiresInSeconds)
{
    m_retryAttempt = 0;
    m_api->setAccessToken(accessToken);
    m_window->setDeviceId(deviceId);

    if (m_signaling->state() == SignalingClient::State::Disconnected)
        m_signaling->connectToServer(m_settings->serverUrl(), accessToken);
    else
        m_signaling->setAccessToken(accessToken);
    m_window->setServerState(m_signaling->isConnected() ? MainWindow::ServerState::Online
                                                        : MainWindow::ServerState::Connecting);

    // Refresh well before expiry so reconnects always have a valid token.
    const int refreshMs = std::max(60, expiresInSeconds * 8 / 10) * 1000;
    m_refreshTimer.start(refreshMs);
    qCInfo(lcApp) << "Authenticated as" << deviceId << "- token refresh in" << refreshMs / 1000 << "s";
}

void Application::scheduleRetry()
{
    const int delay = std::min(1000 * (1 << std::min(m_retryAttempt++, 5)), kMaxRetryDelayMs);
    m_retryTimer.start(delay);
}

} // namespace anypc
