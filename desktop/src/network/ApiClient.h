#pragma once

#include <QJsonValue>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace anypc {

struct ApiResponse
{
    int status = 0;          ///< HTTP status, 0 for transport errors.
    QJsonValue body;         ///< Parsed JSON body (may be undefined).
    QString errorMessage;    ///< Human readable error, empty on success.

    bool ok() const { return status >= 200 && status < 300; }
};

using ApiCallback = std::function<void(const ApiResponse &)>;

/**
 * JSON REST client for the AnyPC backend (`/api/v1`). Requests are
 * authenticated with the device access token once it is set.
 */
class ApiClient final : public QObject
{
    Q_OBJECT

public:
    explicit ApiClient(QObject *parent = nullptr);

    void setBaseUrl(const QUrl &serverUrl);
    QUrl baseUrl() const { return m_baseUrl; }

    void setAccessToken(const QString &token);
    QString accessToken() const { return m_accessToken; }

    /**
     * Issues a request. `path` is relative to `/api/v1` (e.g. "/devices/register").
     * The callback is invoked exactly once on the caller's thread, unless the
     * optional context object is destroyed first.
     */
    void request(const QByteArray &method, const QString &path, const QJsonValue &body,
                 ApiCallback callback, QObject *context = nullptr);

    void get(const QString &path, ApiCallback callback, QObject *context = nullptr);
    void post(const QString &path, const QJsonValue &body, ApiCallback callback, QObject *context = nullptr);

Q_SIGNALS:
    /** Emitted on any 401 so the owner can re-authenticate. */
    void unauthorized();

private:
    QUrl endpoint(const QString &path) const;

    QNetworkAccessManager *m_network;
    QUrl m_baseUrl;
    QString m_accessToken;
};

} // namespace anypc
