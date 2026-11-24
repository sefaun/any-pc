#include "rtc/RtcPage.h"

#include "rtc/RtcBridge.h"
#include "rtc/WebEngineEnvironment.h"

#include <QColor>
#include <QLoggingCategory>
#include <QWebChannel>

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
#include <QWebEngineDesktopMediaRequest>
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <QWebEnginePermission>
#endif

Q_LOGGING_CATEGORY(lcWebConsole, "anypc.web")

namespace anypc {

RtcPage::RtcPage(RtcBridge *bridge, QObject *parent)
    : QWebEnginePage(WebEngineEnvironment::profile(), parent)
    , m_channel(new QWebChannel(this))
{
    m_channel->registerObject(QStringLiteral("bridge"), bridge);
    setWebChannel(m_channel);
    setBackgroundColor(QColor(Qt::black));
    setupPermissions();
}

void RtcPage::loadApp(const QUrl &url)
{
    load(url);
}

bool RtcPage::isTrustedOrigin(const QUrl &origin) const
{
    return origin.scheme() == QLatin1String("qrc");
}

void RtcPage::setupPermissions()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    connect(this, &QWebEnginePage::permissionRequested, this, [this](QWebEnginePermission permission) {
        const auto type = permission.permissionType();
        const bool capture = type == QWebEnginePermission::PermissionType::DesktopVideoCapture
                             || type == QWebEnginePermission::PermissionType::DesktopAudioVideoCapture;
        if (capture && isTrustedOrigin(permission.origin()))
            permission.grant();
        else
            permission.deny();
    });
#else
    connect(this, &QWebEnginePage::featurePermissionRequested, this,
            [this](const QUrl &origin, QWebEnginePage::Feature feature) {
                const bool capture = feature == QWebEnginePage::DesktopVideoCapture
                                     || feature == QWebEnginePage::DesktopAudioVideoCapture;
                setFeaturePermission(origin, feature,
                                     capture && isTrustedOrigin(origin)
                                         ? QWebEnginePage::PermissionGrantedByUser
                                         : QWebEnginePage::PermissionDeniedByUser);
            });
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    // Unattended capture: always pick the first (primary) screen without UI.
    connect(this, &QWebEnginePage::desktopMediaRequested, this,
            [](const QWebEngineDesktopMediaRequest &request) {
                const QAbstractListModel *screens = request.screensModel();
                if (screens && screens->rowCount() > 0)
                    request.selectScreen(screens->index(0));
                else
                    request.cancel();
            });
#endif
}

void RtcPage::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString &message,
                                       int lineNumber, const QString &sourceId)
{
    const QString where = sourceId.section(QLatin1Char('/'), -1) + QLatin1Char(':') + QString::number(lineNumber);
    switch (level) {
    case InfoMessageLevel:
        qCInfo(lcWebConsole).noquote() << where << message;
        break;
    case WarningMessageLevel:
        qCWarning(lcWebConsole).noquote() << where << message;
        break;
    case ErrorMessageLevel:
        qCCritical(lcWebConsole).noquote() << where << message;
        break;
    }
}

bool RtcPage::acceptNavigationRequest(const QUrl &url, NavigationType, bool isMainFrame)
{
    // The page is an embedded application: never navigate away from bundled content.
    return !isMainFrame || url.scheme() == QLatin1String("qrc");
}

} // namespace anypc
