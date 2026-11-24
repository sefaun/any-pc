#pragma once

#include "session/RtcSession.h"

#include <memory>

namespace anypc {

class InputInjector;

/**
 * The controlled side: captures and publishes the screen through the SFU in
 * an off-screen page, and injects input received from the viewer.
 */
class HostSession final : public RtcSession
{
    Q_OBJECT

public:
    HostSession(const QString &connectionId, const protocol::DeviceSummary &peer, ApiClient *api,
                AppSettings *settings, QObject *parent = nullptr);
    ~HostSession() override;

    protocol::Role role() const override { return protocol::Role::Host; }

protected:
    QUrl appUrl() const override;
    QJsonObject startConfig() const override;
    void attachPage(RtcPage *page) override;
    void onStopping() override;

private:
    std::unique_ptr<InputInjector> m_injector;
};

} // namespace anypc
