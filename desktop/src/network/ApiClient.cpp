#include "network/ApiClient.h"

#include "BuildInfo.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace anypc {

namespace {
constexpr int kRequestTimeoutMs = 20000;

QString extractErrorMessage(const QJsonValue &body, QNetworkReply *reply)
{
    const QJsonObject obj = body.toObject();
    const QJsonValue message = obj.value(QLatin1String("message"));
    if (message.isString())
        return message.toString();
    if (message.isArray() && !message.toArray().isEmpty())
        return message.toArray().first().toString();
    return reply->errorString();
}
} // namespace

ApiClient::ApiClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    m_network->setTransferTimeout(kRequestTimeoutMs);
}

void ApiClient::setBaseUrl(const QUrl &serverUrl)
{
    m_baseUrl = serverUrl;
    QString path = m_baseUrl.path();
    while (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    m_baseUrl.setPath(path);
}

void ApiClient::setAccessToken(const QString &token)
{
    m_accessToken = token;
}

QUrl ApiClient::endpoint(const QString &path) const
{
    QUrl url = m_baseUrl;
    const QString normalized = path.startsWith(QLatin1Char('/')) ? path : QLatin1Char('/') + path;
    url.setPath(url.path() + QStringLiteral("/api/v1") + normalized);
    return url;
}

void ApiClient::get(const QString &path, ApiCallback callback, QObject *context)
{
    request("GET", path, QJsonValue::Undefined, std::move(callback), context);
}

void ApiClient::post(const QString &path, const QJsonValue &body, ApiCallback callback, QObject *context)
{
    request("POST", path, body, std::move(callback), context);
}

void ApiClient::request(const QByteArray &method, const QString &path, const QJsonValue &body,
                        ApiCallback callback, QObject *context)
{
    QNetworkRequest req(endpoint(path));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("AnyPC/%1").arg(QString::fromLatin1(build::kVersion)));
    req.setRawHeader("Accept", "application/json");
    if (!m_accessToken.isEmpty())
        req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());

    QByteArray payload;
    if (!body.isUndefined() && !body.isNull()) {
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        payload = body.isObject() ? QJsonDocument(body.toObject()).toJson(QJsonDocument::Compact)
                                  : QJsonDocument(body.toArray()).toJson(QJsonDocument::Compact);
    }

    QNetworkReply *reply = m_network->sendCustomRequest(req, method, payload);
    QPointer<QObject> guard(context ? context : this);
    const bool hasContext = context != nullptr;

    connect(reply, &QNetworkReply::finished, this, [this, reply, guard, hasContext, callback = std::move(callback)]() {
        reply->deleteLater();
        ApiResponse response;
        response.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray raw = reply->readAll();
        if (!raw.isEmpty()) {
            QJsonParseError parseError{};
            const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);
            if (parseError.error == QJsonParseError::NoError)
                response.body = doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array());
        }
        if (!response.ok())
            response.errorMessage = extractErrorMessage(response.body, reply);
        if (response.status == 401)
            Q_EMIT unauthorized();
        if ((hasContext && guard.isNull()) || !callback)
            return;
        callback(response);
    });
}

} // namespace anypc
