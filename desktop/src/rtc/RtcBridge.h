#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QString>

namespace anypc {

class ApiClient;

/**
 * The contract between native code and the embedded WebRTC application
 * (resources/web). Exposed to JavaScript as `bridge` via QWebChannel.
 *
 * JavaScript -> C++ : Q_INVOKABLE methods.
 * C++ -> JavaScript : signals.
 */
class RtcBridge final : public QObject
{
    Q_OBJECT

public:
    RtcBridge(ApiClient *api, const QString &connectionId, QObject *parent = nullptr);

    /** Configuration delivered to the page once it reports readiness. */
    void setStartConfig(const QJsonObject &config) { m_startConfig = config; }
    bool isPageReady() const { return m_pageReady; }

    // ---- Called from JavaScript --------------------------------------------------
    /** Page finished loading and installed its handlers. */
    Q_INVOKABLE void pageReady();
    /** Authenticated backend call restricted to this connection's RTC endpoints. */
    Q_INVOKABLE void apiRequest(int requestId, const QString &method, const QString &path, const QJsonValue &body);
    /** Relays an application signal to the remote peer via the signaling server. */
    Q_INVOKABLE void sendSignal(const QJsonObject &payload);
    /** Reports the session state ("connecting", "connected", "failed", ...). */
    Q_INVOKABLE void reportState(const QString &state, const QString &detail);
    /** Host only: an input event received from the controlling peer. */
    Q_INVOKABLE void inputEvent(const QJsonObject &event);
    /** Host only: the size of the captured screen in pixels. */
    Q_INVOKABLE void captureStarted(int width, int height);
    /** Clipboard text received from the peer over the data channel. */
    Q_INVOKABLE void remoteClipboard(const QString &text);
    Q_INVOKABLE void log(const QString &level, const QString &message);

Q_SIGNALS:
    // ---- Delivered to JavaScript -------------------------------------------------
    void start(const QJsonObject &config);
    /** `body` is serialised JSON: QWebChannel does not marshal QJsonValue signal arguments reliably. */
    void apiResponse(int requestId, int status, const QString &body, const QString &error);
    void peerSignal(const QJsonObject &payload);
    void localClipboard(const QString &text);
    void stop();

    // ---- Native side notifications ----------------------------------------------
    void signalToPeer(const QJsonObject &payload);
    void stateReported(const QString &state, const QString &detail);
    void inputReceived(const QJsonObject &event);
    void captureSizeChanged(int width, int height);
    void remoteClipboardReceived(const QString &text);

private:
    bool isAllowedPath(const QString &method, const QString &path) const;

    ApiClient *m_api;
    QString m_connectionId;
    QJsonObject m_startConfig;
    bool m_pageReady = false;
};

} // namespace anypc
