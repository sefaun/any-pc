#include "rtc/RtcBridge.h"

#include "network/ApiClient.h"
#include "transfer/FileTransferManager.h"

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

void RtcBridge::setFileTransfers(FileTransferManager *files)
{
    m_files = files;
    connect(files, &FileTransferManager::offerFile, this, &RtcBridge::fileOffer);
}

void RtcBridge::captureStarted(int width, int height, const QString &thumbnail)
{
    Q_EMIT captureChanged(width, height, QByteArray::fromBase64(thumbnail.toLatin1()));
}

bool RtcBridge::selectScreen(int index)
{
    // Direct connection: the capture parameters are updated before we return.
    Q_EMIT screenSelectionRequested(index);
    return true;
}

void RtcBridge::remoteScreens(const QJsonObject &info)
{
    Q_EMIT remoteScreensReceived(info);
}

void RtcBridge::remoteAudio(bool available, bool enabled)
{
    Q_EMIT remoteAudioChanged(available, enabled);
}

void RtcBridge::audioState(bool available, bool enabled)
{
    Q_EMIT hostAudioChanged(available, enabled);
}

void RtcBridge::fileChannelReady(bool ready)
{
    if (m_files)
        m_files->setChannelReady(ready);
}

QString RtcBridge::fileReadChunk(const QString &id)
{
    return m_files ? m_files->readChunk(id) : QString();
}

void RtcBridge::fileSendFinished(const QString &id, bool ok, const QString &error)
{
    if (m_files)
        m_files->finishOutgoing(id, ok, error);
}

bool RtcBridge::fileIncoming(const QString &id, const QString &name, double size)
{
    return m_files && m_files->beginIncoming(id, name, static_cast<qint64>(size));
}

bool RtcBridge::fileWriteChunk(const QString &id, const QString &data)
{
    return m_files && m_files->writeChunk(id, data);
}

bool RtcBridge::fileIncomingFinished(const QString &id)
{
    return m_files && m_files->finishIncoming(id);
}

void RtcBridge::fileIncomingFailed(const QString &id, const QString &error)
{
    if (m_files)
        m_files->failIncoming(id, error);
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
