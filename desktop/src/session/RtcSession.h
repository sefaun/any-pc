#pragma once

#include "network/Protocol.h"

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QUrl>

namespace anypc {

class ApiClient;
class AppSettings;
class ClipboardSync;
class RtcBridge;
class RtcPage;

/**
 * Base class for one side of a remote desktop connection.
 *
 * Owns the embedded WebRTC page and its bridge, relays application signals
 * between the page and the signaling server, and synchronises the clipboard.
 * Subclasses provide the page URL, start configuration and role specific
 * behaviour (screen publishing + input injection, or viewing + input capture).
 */
class RtcSession : public QObject
{
    Q_OBJECT

public:
    ~RtcSession() override;

    QString connectionId() const { return m_connectionId; }
    const protocol::DeviceSummary &peer() const { return m_peer; }
    virtual protocol::Role role() const = 0;
    QString state() const { return m_state; }

    /** Creates the page and loads the WebRTC application. */
    void start();
    /** Delivers an application signal sent by the peer. */
    void deliverPeerSignal(const QJsonObject &payload);
    /** Tears the session down. Idempotent; emits finished() once. */
    void stop(const QString &reason);

Q_SIGNALS:
    void stateChanged(const QString &state, const QString &detail);
    /** Sends an application signal to the peer (wired to the signaling client). */
    void outgoingSignal(const QString &connectionId, const QJsonObject &payload);
    /** Emitted once when the session ends, for whatever reason. */
    void finished(const QString &connectionId, const QString &reason);
    void warning(const QString &message);

protected:
    RtcSession(const QString &connectionId, const protocol::DeviceSummary &peer, ApiClient *api,
               AppSettings *settings, QObject *parent);

    virtual QUrl appUrl() const = 0;
    virtual QJsonObject startConfig() const = 0;
    /** Called after the page has been created, before it loads. */
    virtual void attachPage(RtcPage *page) = 0;
    /** Called while stopping, before the page is torn down. */
    virtual void onStopping() {}

    RtcBridge *bridge() const { return m_bridge; }
    ClipboardSync *clipboard() const { return m_clipboard; }
    AppSettings *settings() const { return m_settings; }
    bool isStopping() const { return m_stopping; }

private:
    void flushPendingSignals();

    QString m_connectionId;
    protocol::DeviceSummary m_peer;
    AppSettings *m_settings;
    RtcBridge *m_bridge;
    ClipboardSync *m_clipboard;
    QPointer<RtcPage> m_page;
    QList<QJsonObject> m_pendingSignals;
    QString m_state = QStringLiteral("idle");
    bool m_stopping = false;
};

} // namespace anypc
