#include "session/ViewerSession.h"

#include "core/DeviceId.h"
#include "rtc/RtcBridge.h"
#include "rtc/RtcPage.h"
#include "session/ClipboardSync.h"
#include "ui/ViewerWindow.h"

namespace anypc {

ViewerSession::ViewerSession(const QString &connectionId, const protocol::DeviceSummary &peer, ApiClient *api,
                             AppSettings *settings, QObject *parent)
    : RtcSession(connectionId, peer, api, settings, parent)
{
    const QString title = tr("%1 (%2) - AnyPC").arg(peer.name, DeviceId::format(peer.id));
    m_window = new ViewerWindow(title);
    m_window->setAttribute(Qt::WA_DeleteOnClose, false);

    connect(m_window, &ViewerWindow::disconnectRequested, this, [this]() { stopByUser(tr("You ended the session.")); });
    connect(m_window, &ViewerWindow::sendClipboardRequested, this, [this]() {
        const QString text = clipboard()->currentText();
        if (!text.isEmpty())
            Q_EMIT bridge()->localClipboard(text);
    });
    connect(this, &RtcSession::stateChanged, m_window, &ViewerWindow::setSessionState);
    connect(m_window, &ViewerWindow::sendFilesRequested, this, [this]() { chooseAndSendFiles(m_window); });
    connect(m_window, &ViewerWindow::showTransfersRequested, this, &RtcSession::showFileTransfers);
    connect(m_window, &ViewerWindow::screenSelected, bridge(), &RtcBridge::screenRequested);
    connect(bridge(), &RtcBridge::remoteScreensReceived, m_window, &ViewerWindow::setRemoteScreens);

    // Sound: the remote computer's user controls it; the window only shows the state.
    connect(bridge(), &RtcBridge::remoteAudioChanged, m_window, &ViewerWindow::setRemoteAudio);
}

ViewerSession::~ViewerSession()
{
    delete m_window;
}

void ViewerSession::raiseWindow()
{
    if (!m_window)
        return;
    m_window->showNormal();
    m_window->raise();
    m_window->activateWindow();
}

QUrl ViewerSession::appUrl() const
{
    return QUrl(QStringLiteral("qrc:/web/viewer.html"));
}

QJsonObject ViewerSession::startConfig() const
{
    return {
        {QStringLiteral("role"), QStringLiteral("viewer")},
        {QStringLiteral("connectionId"), connectionId()},
        {QStringLiteral("peerName"), peer().name},
    };
}

void ViewerSession::attachPage(RtcPage *page)
{
    m_window->setPage(page);
    m_window->show();
    m_window->raise();
    m_window->activateWindow();
}

void ViewerSession::onStopping()
{
    if (m_window)
        m_window->hide();
}

} // namespace anypc
