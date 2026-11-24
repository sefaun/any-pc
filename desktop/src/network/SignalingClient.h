#pragma once

#include "network/Protocol.h"

#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>

namespace anypc {

/**
 * WebSocket client for the signaling gateway (`/ws`).
 *
 * Handles authentication, automatic reconnection with exponential backoff
 * and (de)serialisation of protocol frames into typed Qt signals.
 */
class SignalingClient final : public QObject
{
    Q_OBJECT

public:
    enum class State { Disconnected, Connecting, Connected };
    Q_ENUM(State)

    explicit SignalingClient(QObject *parent = nullptr);
    ~SignalingClient() override;

    /** Opens (or re-opens) the socket using the given server and token. */
    void connectToServer(const QUrl &serverUrl, const QString &accessToken);
    /** Updates the token used for subsequent reconnects. */
    void setAccessToken(const QString &accessToken);
    void disconnectFromServer();

    State state() const { return m_state; }
    bool isConnected() const { return m_state == State::Connected; }

    void requestConnection(const QString &targetId, const QString &password, const QString &requestId);
    void respondToConnection(const QString &connectionId, bool accept);
    void endConnection(const QString &connectionId);
    void sendSignal(const QString &connectionId, const QJsonObject &payload);

Q_SIGNALS:
    void stateChanged(anypc::SignalingClient::State state);
    void authenticationFailed();
    void sessionReady(const anypc::protocol::DeviceSummary &self);
    void connectionPending(const QString &requestId, const QString &connectionId,
                           const anypc::protocol::DeviceSummary &target, int expiresInMs);
    void connectionIncoming(const QString &connectionId, const anypc::protocol::DeviceSummary &from,
                            const QString &password, int expiresInMs);
    void connectionAccepted(const QString &connectionId, anypc::protocol::Role role,
                            const anypc::protocol::DeviceSummary &peer);
    void connectionRejected(const QString &connectionId, const QString &reason);
    void connectionEnded(const QString &connectionId, const QString &reason);
    void signalReceived(const QString &connectionId, const QJsonObject &payload);
    void serverError(const QString &requestId, const QString &code, const QString &message);

private:
    void open();
    void setState(State state);
    void scheduleReconnect();
    void send(QLatin1String event, const QJsonObject &data);
    void handleFrame(const QString &message);
    void handleClosed();

    QWebSocket m_socket;
    QTimer m_reconnectTimer;
    QUrl m_serverUrl;
    QString m_accessToken;
    State m_state = State::Disconnected;
    int m_reconnectAttempt = 0;
    bool m_shouldConnect = false;
};

} // namespace anypc
