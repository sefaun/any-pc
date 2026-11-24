#include "rtc/RtcBridge.h"

#include "network/ApiClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QRegularExpression>

Q_LOGGING_CATEGORY(lcBridge, "anypc.bridge")

namespace anypc {

RtcBridge::RtcBridge(ApiClient *api, const QString &connectionId, QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_connectionId(connectionId)
{
}

void RtcBridge::pageReady()
{
    m_pageReady = true;
    Q_EMIT start(m_startConfig);
}

bool RtcBridge::isAllowedPath(const QString &method, const QString &path) const
{
    if (method == QLatin1String("GET") && path == QLatin1String("/rtc/ice-servers"))
        return true;
    // Only this connection's SFU proxy endpoints; no traversal or query strings.
    static const QRegularExpression rtcPath(QStringLiteral("^/rtc/[a-z/]{1,40}$"));
    const QString prefix = QStringLiteral("/connections/") + m_connectionId;
    if (!path.startsWith(prefix))
        return false;
    const QString rest = path.mid(prefix.size());
    return rtcPath.match(rest).hasMatch() && !rest.contains(QLatin1String("//"))
           && (method == QLatin1String("POST") || method == QLatin1String("PUT"));
}

void RtcBridge::apiRequest(int requestId, const QString &method, const QString &path, const QJsonValue &body)
{
    const QString verb = method.toUpper();
    if (!isAllowedPath(verb, path)) {
        qCWarning(lcBridge) << "Blocked API request from page:" << verb << path;
        Q_EMIT apiResponse(requestId, 0, QString(), QStringLiteral("Request not allowed"));
        return;
    }
    m_api->request(verb.toLatin1(), path, body, [this, requestId](const ApiResponse &response) {
        QString json;
        if (response.body.isObject())
            json = QString::fromUtf8(QJsonDocument(response.body.toObject()).toJson(QJsonDocument::Compact));
        else if (response.body.isArray())
            json = QString::fromUtf8(QJsonDocument(response.body.toArray()).toJson(QJsonDocument::Compact));
        Q_EMIT apiResponse(requestId, response.status, json, response.errorMessage);
    }, this);
}

void RtcBridge::sendSignal(const QJsonObject &payload)
{
    Q_EMIT signalToPeer(payload);
}

void RtcBridge::reportState(const QString &state, const QString &detail)
{
    qCInfo(lcBridge).noquote() << "state:" << state << detail;
    Q_EMIT stateReported(state, detail);
}

void RtcBridge::inputEvent(const QJsonObject &event)
{
    Q_EMIT inputReceived(event);
}

void RtcBridge::captureStarted(int width, int height)
{
    Q_EMIT captureSizeChanged(width, height);
}

void RtcBridge::remoteClipboard(const QString &text)
{
    Q_EMIT remoteClipboardReceived(text);
}

void RtcBridge::log(const QString &level, const QString &message)
{
    if (level == QLatin1String("error"))
        qCCritical(lcBridge).noquote() << message;
    else if (level == QLatin1String("warn"))
        qCWarning(lcBridge).noquote() << message;
    else
        qCInfo(lcBridge).noquote() << message;
}

} // namespace anypc
