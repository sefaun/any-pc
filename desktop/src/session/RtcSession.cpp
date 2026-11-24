#include "session/RtcSession.h"

#include "core/AppSettings.h"
#include "rtc/RtcBridge.h"
#include "rtc/RtcPage.h"
#include "session/ClipboardSync.h"

#include <QTimer>

#include <utility>

namespace anypc {

namespace {
/** Time given to the page to close its PeerConnection gracefully. */
constexpr int kTeardownGraceMs = 750;
} // namespace

RtcSession::RtcSession(const QString &connectionId, const protocol::DeviceSummary &peer, ApiClient *api,
                       AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_connectionId(connectionId)
    , m_peer(peer)
    , m_settings(settings)
    , m_bridge(new RtcBridge(api, connectionId, this))
    , m_clipboard(new ClipboardSync(this))
{
    connect(m_bridge, &RtcBridge::signalToPeer, this,
            [this](const QJsonObject &payload) { Q_EMIT outgoingSignal(m_connectionId, payload); });

    connect(m_bridge, &RtcBridge::stateReported, this, [this](const QString &state, const QString &detail) {
        m_state = state;
        Q_EMIT stateChanged(state, detail);
        if (state == QLatin1String("failed"))
            stop(detail.isEmpty() ? QStringLiteral("Connection failed") : detail);
    });

    // Flush buffered peer signals right after the start config reached the page.
    connect(m_bridge, &RtcBridge::start, this, &RtcSession::flushPendingSignals, Qt::QueuedConnection);

    connect(m_bridge, &RtcBridge::remoteClipboardReceived, m_clipboard, &ClipboardSync::applyRemote);
    connect(m_clipboard, &ClipboardSync::localTextChanged, m_bridge, &RtcBridge::localClipboard);
}

RtcSession::~RtcSession() = default;

void RtcSession::start()
{
    m_clipboard->setEnabled(m_settings->clipboardSyncEnabled());
    m_bridge->setStartConfig(startConfig());
    m_page = new RtcPage(m_bridge, this);
    attachPage(m_page);
    m_page->loadApp(appUrl());
}

void RtcSession::deliverPeerSignal(const QJsonObject &payload)
{
    if (m_stopping)
        return;
    if (!m_bridge->isPageReady()) {
        m_pendingSignals.append(payload);
        return;
    }
    Q_EMIT m_bridge->peerSignal(payload);
}

void RtcSession::flushPendingSignals()
{
    const QList<QJsonObject> pending = std::exchange(m_pendingSignals, {});
    for (const QJsonObject &payload : pending)
        Q_EMIT m_bridge->peerSignal(payload);
}

void RtcSession::stop(const QString &reason)
{
    if (m_stopping)
        return;
    m_stopping = true;
    m_clipboard->setEnabled(false);
    onStopping();
    Q_EMIT m_bridge->stop();
    m_state = QStringLiteral("closed");
    Q_EMIT stateChanged(m_state, reason);

    // Let the page close its PeerConnection, then release Chromium resources.
    QTimer::singleShot(kTeardownGraceMs, this, [this]() {
        if (m_page)
            m_page->deleteLater();
    });
    Q_EMIT finished(m_connectionId, reason);
}

} // namespace anypc
