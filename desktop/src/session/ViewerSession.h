#pragma once

#include "session/RtcSession.h"

#include <QPointer>

namespace anypc {

class ViewerWindow;

/**
 * The controlling side: shows the remote screen in a window and forwards
 * local keyboard/mouse input over the SFU data channel.
 */
class ViewerSession final : public RtcSession
{
    Q_OBJECT

public:
    ViewerSession(const QString &connectionId, const protocol::DeviceSummary &peer, ApiClient *api,
                  AppSettings *settings, QObject *parent = nullptr);
    ~ViewerSession() override;

    protocol::Role role() const override { return protocol::Role::Viewer; }
    void raiseWindow();

protected:
    QUrl appUrl() const override;
    QJsonObject startConfig() const override;
    void attachPage(RtcPage *page) override;
    void onStopping() override;

private:
    QPointer<ViewerWindow> m_window;
};

} // namespace anypc
