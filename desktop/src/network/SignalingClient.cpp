#include "network/SignalingClient.h"

#include <QJsonDocument>
#include <QLoggingCategory>
#include <QNetworkRequest>
#include <QRandomGenerator>

#include <algorithm>

Q_LOGGING_CATEGORY(lcSignaling, "anypc.signaling")

namespace anypc {

namespace {
constexpr int kBaseBackoffMs = 1000;
constexpr int kMaxBackoffMs = 30000;

QUrl toWebSocketUrl(const QUrl &serverUrl)
{
    QUrl url = serverUrl;
    QString path = url.path();
    while (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    url.setScheme(url.scheme() == QLatin1String("https") ? QStringLiteral("wss") : QStringLiteral("ws"));
    url.setPath(path + QStringLiteral("/ws"));
    return url;
}
} // namespace

SignalingClient::SignalingClient(QObject *parent)
    : QObject(parent)
{
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &SignalingClient::open);

    connect(&m_socket, &QWebSocket::connected, this, [this]() {
        qCInfo(lcSignaling) << "Socket connected; awaiting session.ready";
    });
    connect(&m_socket, &QWebSocket::disconnected, this, &SignalingClient::handleClosed);
    connect(&m_socket, &QWebSocket::textMessageReceived, this, &SignalingClient::handleFrame);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(&m_socket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        qCWarning(lcSignaling) << "Socket error:" << m_socket.errorString();
    });
#else
    connect(&m_socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this,
            [this](QAbstractSocket::SocketError) {
                qCWarning(lcSignaling) << "Socket error:" << m_socket.errorString();
            });
#endif
}

SignalingClient::~SignalingClient()
{
    m_shouldConnect = false;
    m_socket.abort();
}

void SignalingClient::connectToServer(const QUrl &serverUrl, const QString &accessToken)
{
    m_serverUrl = serverUrl;
    m_accessToken = accessToken;
    m_shouldConnect = true;
    m_reconnectAttempt = 0;
    m_reconnectTimer.stop();
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        // handleClosed() schedules a reconnect with the new parameters.
        m_socket.close();
        return;
    }
    open();
}

void SignalingClient::setAccessToken(const QString &accessToken)
{
    m_accessToken = accessToken;
}

void SignalingClient::disconnectFromServer()
{
    m_shouldConnect = false;
    m_reconnectTimer.stop();
    m_socket.close();
    setState(State::Disconnected);
}

void SignalingClient::open()
{
    if (!m_shouldConnect || m_accessToken.isEmpty())
        return;
    QNetworkRequest request(toWebSocketUrl(m_serverUrl));
    request.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
    setState(State::Connecting);
    m_socket.open(request);
}

void SignalingClient::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    Q_EMIT stateChanged(state);
}

void SignalingClient::handleClosed()
{
    const int code = static_cast<int>(m_socket.closeCode());
    qCInfo(lcSignaling) << "Socket closed, code" << code << m_socket.closeReason();
    setState(State::Disconnected);

    if (code == protocol::kCloseUnauthorized) {
        // Owner must obtain a new token and call connectToServer() again.
        Q_EMIT authenticationFailed();
        return;
    }
    if (code == protocol::kCloseReplaced) {
        qCWarning(lcSignaling) << "Another instance took over this device id; not reconnecting";
        m_shouldConnect = false;
        return;
    }
    scheduleReconnect();
}

void SignalingClient::scheduleReconnect()
{
    if (!m_shouldConnect || m_reconnectTimer.isActive())
        return;
    const int exp = std::min(m_reconnectAttempt++, 5);
    const int base = std::min(kBaseBackoffMs * (1 << exp), kMaxBackoffMs);
    const int jitter = QRandomGenerator::global()->bounded(base / 4 + 1);
    m_reconnectTimer.start(base + jitter);
}

void SignalingClient::send(QLatin1String event, const QJsonObject &data)
{
    if (m_socket.state() != QAbstractSocket::ConnectedState) {
        qCWarning(lcSignaling) << "Dropping" << event << "- socket not connected";
        return;
    }
    const QJsonObject frame{{QStringLiteral("event"), QString(event)}, {QStringLiteral("data"), data}};
    m_socket.sendTextMessage(QString::fromUtf8(QJsonDocument(frame).toJson(QJsonDocument::Compact)));
}

void SignalingClient::requestConnection(const QString &targetId, const QString &password,
                                        const QString &requestId)
{
    QJsonObject data{{QStringLiteral("targetId"), targetId}, {QStringLiteral("requestId"), requestId}};
    if (!password.isEmpty())
        data.insert(QStringLiteral("password"), password);
    send(protocol::client::ConnectionRequest, data);
}

void SignalingClient::respondToConnection(const QString &connectionId, bool accept)
{
    send(protocol::client::ConnectionRespond,
         {{QStringLiteral("connectionId"), connectionId}, {QStringLiteral("accept"), accept}});
}

void SignalingClient::endConnection(const QString &connectionId)
{
    send(protocol::client::ConnectionEnd, {{QStringLiteral("connectionId"), connectionId}});
}

void SignalingClient::sendSignal(const QString &connectionId, const QJsonObject &payload)
{
    send(protocol::client::ConnectionSignal,
         {{QStringLiteral("connectionId"), connectionId}, {QStringLiteral("payload"), payload}});
}

void SignalingClient::handleFrame(const QString &message)
{
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(lcSignaling) << "Ignoring malformed frame";
        return;
    }
    const QJsonObject frame = doc.object();
    const QString event = frame.value(QLatin1String("event")).toString();
    const QJsonObject data = frame.value(QLatin1String("data")).toObject();
    const auto str = [&data](const char *key) { return data.value(QLatin1String(key)).toString(); };
    const auto device = [&data](const char *key) {
        return protocol::DeviceSummary::fromJson(data.value(QLatin1String(key)).toObject());
    };

    using namespace protocol::server;
    if (event == SessionReady) {
        m_reconnectAttempt = 0;
        setState(State::Connected);
        Q_EMIT sessionReady(device("device"));
    } else if (event == ConnectionPending) {
        Q_EMIT connectionPending(str("requestId"), str("connectionId"), device("target"),
                                 data.value(QLatin1String("expiresInMs")).toInt());
    } else if (event == ConnectionIncoming) {
        Q_EMIT connectionIncoming(str("connectionId"), device("from"), str("password"),
                                  data.value(QLatin1String("expiresInMs")).toInt());
    } else if (event == ConnectionAccepted) {
        const auto role = str("role") == QLatin1String("host") ? protocol::Role::Host : protocol::Role::Viewer;
        Q_EMIT connectionAccepted(str("connectionId"), role, device("peer"));
    } else if (event == ConnectionRejected) {
        Q_EMIT connectionRejected(str("connectionId"), str("reason"));
    } else if (event == ConnectionEnded) {
        Q_EMIT connectionEnded(str("connectionId"), str("reason"));
    } else if (event == ConnectionSignal) {
        Q_EMIT signalReceived(str("connectionId"), data.value(QLatin1String("payload")).toObject());
    } else if (event == Error) {
        qCWarning(lcSignaling) << "Server error" << str("code") << str("message");
        Q_EMIT serverError(str("requestId"), str("code"), str("message"));
    } else {
        qCDebug(lcSignaling) << "Unhandled event" << event;
    }
}

} // namespace anypc
