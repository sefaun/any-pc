#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QString>

namespace anypc {

class ApiClient;
class FileTransferManager;

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
    void setFileTransfers(FileTransferManager *files);

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
    /**
     * Host only: a capture started. `thumbnail` is a base64 64x36 grayscale
     * preview used to identify the monitor (may be empty).
     */
    Q_INVOKABLE void captureStarted(int width, int height, const QString &thumbnail);
    /** Host only: switch the capture to another screen; returns once applied. */
    Q_INVOKABLE bool selectScreen(int index);
    /** Viewer only: the host's list of screens ({screens:[{index,name}], current}). */
    Q_INVOKABLE void remoteScreens(const QJsonObject &info);
    /** Viewer only: whether the host's sound is received and whether the host has it on. */
    Q_INVOKABLE void remoteAudio(bool available, bool enabled);
    /** Host only: whether this computer's sound could be captured and whether it is on. */
    Q_INVOKABLE void audioState(bool available, bool enabled);

    // File transfer (see FileTransferManager). Methods returning values are
    // called with a callback from JavaScript.
    Q_INVOKABLE void fileChannelReady(bool ready);
    Q_INVOKABLE QString fileReadChunk(const QString &id);
    Q_INVOKABLE void fileSendFinished(const QString &id, bool ok, const QString &error);
    Q_INVOKABLE bool fileIncoming(const QString &id, const QString &name, double size);
    Q_INVOKABLE bool fileWriteChunk(const QString &id, const QString &data);
    Q_INVOKABLE bool fileIncomingFinished(const QString &id);
    Q_INVOKABLE void fileIncomingFailed(const QString &id, const QString &error);
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
    /** Ask the page to send an outgoing file over the files channel. */
    void fileOffer(const QString &id, const QString &name, double size);
    /** Host: screens available for capture ({screens:[{index,name}], current}). */
    void screensUpdated(const QJsonObject &info);
    /** Viewer: the user picked another remote screen. */
    void screenRequested(int index);
    /** Host: the local user turned the shared sound on or off. */
    void audioEnabled(bool enabled);

    // ---- Native side notifications ----------------------------------------------
    void signalToPeer(const QJsonObject &payload);
    void stateReported(const QString &state, const QString &detail);
    void inputReceived(const QJsonObject &event);
    void captureChanged(int width, int height, const QByteArray &thumbnail);
    void screenSelectionRequested(int index);
    void remoteScreensReceived(const QJsonObject &info);
    void remoteAudioChanged(bool available, bool enabled);
    void hostAudioChanged(bool available, bool enabled);
    void remoteClipboardReceived(const QString &text);

private:
    bool isAllowedPath(const QString &method, const QString &path) const;

    ApiClient *m_api;
    QString m_connectionId;
    QJsonObject m_startConfig;
    FileTransferManager *m_files = nullptr;
    bool m_pageReady = false;
};

} // namespace anypc
