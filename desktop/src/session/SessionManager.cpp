#include "session/SessionManager.h"

#include "core/AppSettings.h"
#include "core/DeviceId.h"
#include "network/SignalingClient.h"
#include "session/HostSession.h"
#include "session/ViewerSession.h"

#include <QLoggingCategory>
#include <QTimer>
#include <QUuid>

#include <utility>

Q_LOGGING_CATEGORY(lcSessions, "anypc.sessions")

namespace anypc {

namespace {
/** Keeps a finished session alive briefly so its page can shut down cleanly. */
constexpr int kSessionDisposeDelayMs = 1500;

QString describeEndReason(const QString &reason)
{
    if (reason == QLatin1String("hangup"))
        return SessionManager::tr("The remote side ended the session.");
    if (reason == QLatin1String("peer_disconnected"))
        return SessionManager::tr("The remote device disconnected.");
    if (reason == QLatin1String("cancelled"))
        return SessionManager::tr("The connection request was cancelled.");
    if (reason == QLatin1String("timeout"))
        return SessionManager::tr("The connection request timed out.");
    return SessionManager::tr("Session ended.");
}
} // namespace

SessionManager::SessionManager(ApiClient *api, SignalingClient *signaling, AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_signaling(signaling)
    , m_settings(settings)
{
    connect(m_signaling, &SignalingClient::connectionIncoming, this, &SessionManager::onIncoming);
    connect(m_signaling, &SignalingClient::connectionPending, this,
            [this](const QString &requestId, const QString &connectionId, const protocol::DeviceSummary &target, int) {
                onPending(requestId, connectionId, target);
            });
    connect(m_signaling, &SignalingClient::connectionAccepted, this, &SessionManager::onAccepted);
    connect(m_signaling, &SignalingClient::connectionRejected, this, &SessionManager::onRejected);
    connect(m_signaling, &SignalingClient::connectionEnded, this, &SessionManager::onEnded);
    connect(m_signaling, &SignalingClient::serverError, this, &SessionManager::onServerError);
    connect(m_signaling, &SignalingClient::stateChanged, this, &SessionManager::onSignalingStateChanged);
    connect(m_signaling, &SignalingClient::signalReceived, this,
            [this](const QString &connectionId, const QJsonObject &payload) {
                if (RtcSession *session = m_sessions.value(connectionId))
                    session->deliverPeerSignal(payload);
            });
}

SessionManager::~SessionManager()
{
    for (RtcSession *session : std::as_const(m_sessions))
        session->disconnect(this);
    qDeleteAll(m_sessions);
}

// ---- Outgoing -------------------------------------------------------------------

void SessionManager::connectTo(const QString &deviceId, const QString &password)
{
    if (!m_signaling->isConnected()) {
        Q_EMIT notification(tr("Not connected to the AnyPC server."));
        return;
    }
    if (hasOutgoingRequest()) {
        Q_EMIT notification(tr("A connection request is already in progress."));
        return;
    }
    m_outgoingRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_outgoingTargetId = deviceId;
    m_settings->addRecentConnection(deviceId);
    m_signaling->requestConnection(deviceId, password, m_outgoingRequestId);
}

void SessionManager::cancelOutgoing()
{
    if (!m_outgoingConnectionId.isEmpty())
        m_signaling->endConnection(m_outgoingConnectionId);
    clearOutgoing();
}

void SessionManager::clearOutgoing()
{
    const bool had = hasOutgoingRequest();
    m_outgoingRequestId.clear();
    m_outgoingConnectionId.clear();
    m_outgoingTargetId.clear();
    if (had)
        Q_EMIT outgoingFinished();
}

void SessionManager::onPending(const QString &requestId, const QString &connectionId,
                               const protocol::DeviceSummary &target)
{
    if (requestId != m_outgoingRequestId)
        return;
    m_outgoingConnectionId = connectionId;
    Q_EMIT outgoingPending(target);
}

void SessionManager::onServerError(const QString &requestId, const QString &code, const QString &message)
{
    if (!requestId.isEmpty() && requestId == m_outgoingRequestId) {
        clearOutgoing();
        if (code == QLatin1String("DEVICE_NOT_FOUND"))
            Q_EMIT notification(tr("No device with this ID exists."));
        else if (code == QLatin1String("DEVICE_OFFLINE"))
            Q_EMIT notification(tr("The remote device is offline."));
        else
            Q_EMIT notification(message);
        return;
    }
    qCWarning(lcSessions) << "Server error" << code << message;
}

// ---- Incoming -------------------------------------------------------------------

void SessionManager::onIncoming(const QString &connectionId, const protocol::DeviceSummary &from,
                                const QString &password, int expiresInMs)
{
    if (isBeingControlled()) {
        qCInfo(lcSessions) << "Rejecting" << from.id << "- already being controlled";
        m_signaling->respondToConnection(connectionId, false);
        return;
    }
    if (!password.isEmpty()) {
        // An unattended attempt: accept on a correct password, refuse otherwise.
        const bool valid = m_settings->verifyUnattendedPassword(password);
        qCInfo(lcSessions) << "Unattended request from" << from.id << (valid ? "accepted" : "rejected");
        m_signaling->respondToConnection(connectionId, valid);
        if (!valid)
            Q_EMIT notification(tr("Rejected %1: wrong password.").arg(DeviceId::format(from.id)));
        return;
    }
    m_pendingIncoming.insert(connectionId, from);
    Q_EMIT incomingRequest(connectionId, from, expiresInMs);
}

void SessionManager::respond(const QString &connectionId, bool accept)
{
    if (!m_pendingIncoming.remove(connectionId))
        return;
    m_signaling->respondToConnection(connectionId, accept);
}

// ---- Lifecycle ------------------------------------------------------------------

void SessionManager::onAccepted(const QString &connectionId, protocol::Role role, const protocol::DeviceSummary &peer)
{
    if (m_sessions.contains(connectionId))
        return;

    if (role == protocol::Role::Viewer) {
        if (connectionId == m_outgoingConnectionId)
            clearOutgoing();
        startSession(new ViewerSession(connectionId, peer, m_api, m_settings, this));
    } else {
        startSession(new HostSession(connectionId, peer, m_api, m_settings, this));
        Q_EMIT hostSessionStarted(peer);
    }
}

void SessionManager::startSession(RtcSession *session)
{
    m_sessions.insert(session->connectionId(), session);
    connect(session, &RtcSession::outgoingSignal, m_signaling, &SignalingClient::sendSignal);
    connect(session, &RtcSession::finished, this, &SessionManager::onSessionFinished);
    connect(session, &RtcSession::warning, this, &SessionManager::notification);
    session->start();
}

void SessionManager::onRejected(const QString &connectionId, const QString &reason)
{
    if (connectionId != m_outgoingConnectionId)
        return;
    clearOutgoing();
    Q_EMIT notification(reason == QLatin1String("timeout") ? tr("The remote side did not answer.")
                                                           : tr("The remote side declined the connection."));
}

void SessionManager::onEnded(const QString &connectionId, const QString &reason)
{
    if (m_pendingIncoming.remove(connectionId))
        Q_EMIT incomingRequestClosed(connectionId);
    if (connectionId == m_outgoingConnectionId) {
        clearOutgoing();
        Q_EMIT notification(describeEndReason(reason));
    }
    if (RtcSession *session = m_sessions.value(connectionId))
        session->stop(describeEndReason(reason));
}

void SessionManager::onSessionFinished(const QString &connectionId, const QString &reason)
{
    RtcSession *session = m_sessions.take(connectionId);
    if (!session)
        return;
    qCInfo(lcSessions) << "Session" << connectionId << "finished:" << reason;
    // Idempotent on the server; covers locally initiated endings.
    m_signaling->endConnection(connectionId);
    if (session->role() == protocol::Role::Host)
        Q_EMIT hostSessionEnded();
    Q_EMIT notification(reason);
    if (!session->endedByUser())
        Q_EMIT sessionEndedRemotely(session->peer(), session->role(), reason);
    session->disconnect(this);
    QTimer::singleShot(kSessionDisposeDelayMs, session, &QObject::deleteLater);
}

void SessionManager::endHostSession()
{
    for (RtcSession *session : m_sessions.values()) {
        if (session->role() == protocol::Role::Host)
            session->stopByUser(tr("You ended the session."));
    }
}

void SessionManager::sendFilesToViewer(QWidget *dialogParent)
{
    for (RtcSession *session : std::as_const(m_sessions)) {
        if (session->role() == protocol::Role::Host) {
            session->chooseAndSendFiles(dialogParent);
            return;
        }
    }
}

void SessionManager::showHostTransfers()
{
    for (RtcSession *session : std::as_const(m_sessions)) {
        if (session->role() == protocol::Role::Host) {
            session->showFileTransfers();
            return;
        }
    }
}

bool SessionManager::isBeingControlled() const
{
    for (const RtcSession *session : m_sessions) {
        if (session->role() == protocol::Role::Host)
            return true;
    }
    return false;
}

void SessionManager::endAll()
{
    for (RtcSession *session : m_sessions.values())
        session->stopByUser(tr("Session closed."));
    cancelOutgoing();
}

void SessionManager::onSignalingStateChanged()
{
    if (m_signaling->isConnected())
        return;
    // The server ends every connection of a disconnected device; mirror that locally.
    for (const QString &connectionId : m_pendingIncoming.keys())
        Q_EMIT incomingRequestClosed(connectionId);
    m_pendingIncoming.clear();
    clearOutgoing();
    for (RtcSession *session : m_sessions.values())
        session->stop(tr("Lost connection to the AnyPC server."));
}

} // namespace anypc
