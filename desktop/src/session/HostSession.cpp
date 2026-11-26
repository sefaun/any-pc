#include "session/HostSession.h"

#include "core/AppSettings.h"
#include "input/InputInjector.h"
#include "rtc/RtcBridge.h"
#include "rtc/RtcPage.h"
#include "session/ScreenMatcher.h"
#include "ui/ScreenBorderOverlay.h"

#include <QGuiApplication>
#include <QJsonArray>
#include <QLoggingCategory>
#include <QPixmap>
#include <QRegularExpression>
#include <QScreen>

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

/**
 * System sound is captured with Chromium's loopback device, which exists on
 * Windows and Linux only. On macOS the attempt is skipped entirely so that it
 * cannot disturb the screen capture.
 */
constexpr bool kSystemAudioSupported =
#if defined(Q_OS_MACOS)
    false;
#else
    true;
#endif
/** Errors getUserMedia/getDisplayMedia report when the screen cannot be captured. */
bool isCaptureError(const QString &detail)
{
    static const QRegularExpression pattern(
        QStringLiteral("permission|notallowed|notreadable|notfound|abort|capture|video source|screen"),
        QRegularExpression::CaseInsensitiveOption);
    return detail.isEmpty() || pattern.match(detail).hasMatch();
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
    connect(bridge(), &RtcBridge::captureChanged, this, &HostSession::onCaptureChanged);
    m_audioEnabled = settings->shareAudioEnabled();
    connect(this, &RtcSession::stateChanged, this, [this](const QString &state, const QString &detail) {
        if (state == QLatin1String("failed") && !m_captureStarted && isCaptureError(detail))
            Q_EMIT captureFailed(detail.isEmpty() ? tr("The screen could not be captured.") : detail);
    });
    connect(bridge(), &RtcBridge::hostAudioChanged, this, [this](bool available, bool enabled) {
        m_audioAvailable = available;
        m_audioEnabled = enabled;
        Q_EMIT audioStateChanged(available, enabled);
    });
    connect(bridge(), &RtcBridge::screenSelectionRequested, this, [this](int index) {
        if (page())
            page()->setScreenIndex(index);
    });
    connect(bridge(), &RtcBridge::stateReported, this, [this](const QString &state, const QString &) {
        if (state != QLatin1String("connected"))
            return;
        m_connected = true;
        updateOverlays();
        if (!m_injector->isSupported())
            Q_EMIT warning(m_injector->unsupportedReason());
    });
}

HostSession::~HostSession()
{
    qDeleteAll(m_overlays);
}

void HostSession::onCaptureChanged(int width, int height, const QByteArray &thumbnail)
{
    m_captureStarted = true;
    const QSize size(width, height);
    m_injector->setCaptureSize(size);

    const QList<ScreenMatcher::Candidate> candidates = ScreenMatcher::currentScreens();
    const int index = ScreenMatcher::match(candidates, size, thumbnail, [](const ScreenMatcher::Candidate &c) {
        return c.screen ? c.screen->grabWindow(0).toImage() : QImage();
    });

    m_sharedScreens.clear();
    if (index >= 0) {
        QScreen *screen = candidates.at(index).screen;
        m_sharedScreens.append(screen);
        m_injector->setTargetScreen(screen);
    } else {
        // The capture spans the whole desktop: map input over it and frame every screen.
        for (QScreen *screen : QGuiApplication::screens())
            m_sharedScreens.append(screen);
        m_injector->setTargetScreen(nullptr);
    }
    updateOverlays();
}

void HostSession::onScreensEnumerated(const QStringList &names, int selected)
{
    QJsonArray screens;
    for (int i = 0; i < names.size(); ++i)
        screens.append(QJsonObject{{QStringLiteral("index"), i}, {QStringLiteral("name"), names.at(i)}});
    Q_EMIT bridge()->screensUpdated(QJsonObject{{QStringLiteral("screens"), screens},
                                                {QStringLiteral("current"), selected}});
}

void HostSession::updateOverlays()
{
    if (!m_connected || isStopping())
        return;
    const QString caption = tr("AnyPC · %1 is viewing this screen").arg(peer().name);
    while (m_overlays.size() < m_sharedScreens.size())
        m_overlays.append(new ScreenBorderOverlay());
    for (int i = 0; i < m_overlays.size(); ++i) {
        QScreen *screen = i < m_sharedScreens.size() ? m_sharedScreens.at(i).data() : nullptr;
        m_overlays.at(i)->showOn(screen, screen ? caption : QString());
    }
}

void HostSession::hideOverlays()
{
    for (ScreenBorderOverlay *overlay : std::as_const(m_overlays))
        overlay->hide();
}

void HostSession::setAudioEnabled(bool enabled)
{
    m_audioEnabled = enabled;
    Q_EMIT bridge()->audioEnabled(enabled);
}

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
        {QStringLiteral("captureAudio"), kSystemAudioSupported},
        {QStringLiteral("audioEnabled"), m_audioEnabled},
    };
}

void HostSession::attachPage(RtcPage *page)
{
    // The host page runs off-screen; only its screen enumeration is of interest.
    connect(page, &RtcPage::screensEnumerated, this, &HostSession::onScreensEnumerated);
}

void HostSession::onStopping()
{
    m_injector->releaseAll();
    hideOverlays();
}

} // namespace anypc
