#pragma once

#include "network/Protocol.h"

#include <QHash>
#include <QObject>
#include <QPointer>

class QWidget;

namespace anypc {

class ApiClient;
class AppSettings;
class HostSession;
class RtcSession;
class SignalingClient;

/**
 * Orchestrates connection requests and the lifetime of host/viewer sessions.
 * UI agnostic: it exposes signals that the main window turns into dialogs.
 */
class SessionManager final : public QObject
{
    Q_OBJECT

public:
    SessionManager(ApiClient *api, SignalingClient *signaling, AppSettings *settings, QObject *parent = nullptr);
    ~SessionManager() override;

    /** Requests control of a remote device. `password` enables unattended access. */
    void connectTo(const QString &deviceId, const QString &password);
    void cancelOutgoing();
    bool hasOutgoingRequest() const { return !m_outgoingRequestId.isEmpty() || !m_outgoingConnectionId.isEmpty(); }

    /** Answers an incoming request previously announced by incomingRequest(). */
    void respond(const QString &connectionId, bool accept);

    /** Ends the session in which this device is being controlled, if any. */
    void endHostSession();
    bool isBeingControlled() const;
    /** Host side: send files to the controlling device / show transfers. */
    void sendFilesToViewer(QWidget *dialogParent);
    void showHostTransfers();

    void endAll();

Q_SIGNALS:
    void incomingRequest(const QString &connectionId, const anypc::protocol::DeviceSummary &from, int expiresInMs);
    void incomingRequestClosed(const QString &connectionId);
    void outgoingPending(const anypc::protocol::DeviceSummary &target);
    void outgoingFinished();
    void hostSessionStarted(const anypc::protocol::DeviceSummary &viewer);
    void hostSessionEnded();
    /** A session ended without the local user asking for it (peer left, network, ...). */
    void sessionEndedRemotely(const anypc::protocol::DeviceSummary &peer, anypc::protocol::Role role,
                              const QString &reason);
    void notification(const QString &message);

private:
    void onIncoming(const QString &connectionId, const protocol::DeviceSummary &from, const QString &password,
                    int expiresInMs);
    void onPending(const QString &requestId, const QString &connectionId, const protocol::DeviceSummary &target);
    static QString phoneTargetMessage();
    void onAccepted(const QString &connectionId, protocol::Role role, const protocol::DeviceSummary &peer);
    void onRejected(const QString &connectionId, const QString &reason);
    void onEnded(const QString &connectionId, const QString &reason);
    void onServerError(const QString &requestId, const QString &code, const QString &message);
    void onSignalingStateChanged();

    void startSession(RtcSession *session);
    void onSessionFinished(const QString &connectionId, const QString &reason);
    void clearOutgoing();

    ApiClient *m_api;
    SignalingClient *m_signaling;
    AppSettings *m_settings;

    QHash<QString, RtcSession *> m_sessions;
    QHash<QString, protocol::DeviceSummary> m_pendingIncoming;
    QString m_outgoingRequestId;
    QString m_outgoingConnectionId;
    QString m_outgoingTargetId;
};

} // namespace anypc
