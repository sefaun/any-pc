#pragma once

#include "session/RtcSession.h"

#include <QList>
#include <QPointer>

#include <memory>

class QScreen;

namespace anypc {

class InputInjector;
class ScreenBorderOverlay;

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

    /** Whether this computer's sound is captured, and whether it is currently shared. */
    bool isAudioAvailable() const { return m_audioAvailable; }
    bool isAudioEnabled() const { return m_audioEnabled; }
    /** Turns the shared sound on or off for the viewer (live, no reconnection). */
    void setAudioEnabled(bool enabled);

Q_SIGNALS:
    void audioStateChanged(bool available, bool enabled);
    /** The session failed before the screen could be captured. */
    void captureFailed(const QString &reason);

protected:
    QUrl appUrl() const override;
    QJsonObject startConfig() const override;
    void attachPage(RtcPage *page) override;
    void onStopping() override;

private:
    void onCaptureChanged(int width, int height, const QByteArray &thumbnail);
    void onScreensEnumerated(const QStringList &names, int selected);
    void updateOverlays();
    void hideOverlays();

    std::unique_ptr<InputInjector> m_injector;
    QList<ScreenBorderOverlay *> m_overlays;
    /** Screens shown in the capture (several when the whole desktop is captured). */
    QList<QPointer<QScreen>> m_sharedScreens;
    bool m_connected = false;
    bool m_audioAvailable = false;
    bool m_captureStarted = false;
    bool m_audioEnabled = true;
};

} // namespace anypc
