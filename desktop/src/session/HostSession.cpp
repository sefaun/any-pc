#include "session/HostSession.h"

#include "input/InputInjector.h"
#include "rtc/RtcBridge.h"
#include "rtc/RtcPage.h"

#include <QSize>

namespace anypc {

namespace {
constexpr int kMaxCaptureWidth = 2560;
constexpr int kMaxCaptureHeight = 1600;
constexpr int kMaxFrameRate = 30;
constexpr int kMaxBitrateBps = 6'000'000;

QString platformName()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#else
    return QStringLiteral("linux");
#endif
}
} // namespace

HostSession::HostSession(const QString &connectionId, const protocol::DeviceSummary &peer, ApiClient *api,
                         AppSettings *settings, QObject *parent)
    : RtcSession(connectionId, peer, api, settings, parent)
    , m_injector(InputInjector::create())
{
    connect(bridge(), &RtcBridge::inputReceived, this, [this](const QJsonObject &json) {
        if (const auto event = InputEvent::fromJson(json))
            m_injector->handle(*event);
    });
    connect(bridge(), &RtcBridge::captureSizeChanged, this,
            [this](int width, int height) { m_injector->setCaptureSize(QSize(width, height)); });
    connect(bridge(), &RtcBridge::stateReported, this, [this](const QString &state, const QString &) {
        if (state == QLatin1String("connected") && !m_injector->isSupported())
            Q_EMIT warning(m_injector->unsupportedReason());
    });
}

HostSession::~HostSession() = default;

QUrl HostSession::appUrl() const
{
    return QUrl(QStringLiteral("qrc:/web/host.html"));
}

QJsonObject HostSession::startConfig() const
{
    return {
        {QStringLiteral("role"), QStringLiteral("host")},
        {QStringLiteral("connectionId"), connectionId()},
        {QStringLiteral("platform"), platformName()},
        {QStringLiteral("capture"),
         QJsonObject{{QStringLiteral("maxWidth"), kMaxCaptureWidth},
                     {QStringLiteral("maxHeight"), kMaxCaptureHeight},
                     {QStringLiteral("frameRate"), kMaxFrameRate},
                     {QStringLiteral("maxBitrate"), kMaxBitrateBps}}},
        {QStringLiteral("inputSupported"), m_injector->isSupported()},
    };
}

void HostSession::attachPage(RtcPage *)
{
    // The host page runs off-screen; nothing to embed.
}

void HostSession::onStopping()
{
    m_injector->releaseAll();
}

} // namespace anypc
