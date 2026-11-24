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

    connect(m_window, &ViewerWindow::disconnectRequested, this, [this]() { stop(tr("Disconnected")); });
    connect(m_window, &ViewerWindow::sendClipboardRequested, this, [this]() {
        const QString text = clipboard()->currentText();
        if (!text.isEmpty())
            Q_EMIT bridge()->localClipboard(text);
    });
    connect(this, &RtcSession::stateChanged, m_window, &ViewerWindow::setSessionState);
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
