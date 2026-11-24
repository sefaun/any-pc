#pragma once

#include <QJsonObject>
#include <QLatin1String>
#include <QString>

/**
 * Shared vocabulary of the signaling protocol. Must stay in sync with
 * backend/src/signaling/signaling.protocol.ts.
 */
namespace anypc::protocol {

namespace client {
inline constexpr QLatin1String ConnectionRequest{"connection.request"};
inline constexpr QLatin1String ConnectionRespond{"connection.respond"};
inline constexpr QLatin1String ConnectionEnd{"connection.end"};
inline constexpr QLatin1String ConnectionSignal{"connection.signal"};
} // namespace client

namespace server {
inline constexpr QLatin1String SessionReady{"session.ready"};
inline constexpr QLatin1String ConnectionPending{"connection.pending"};
inline constexpr QLatin1String ConnectionIncoming{"connection.incoming"};
inline constexpr QLatin1String ConnectionAccepted{"connection.accepted"};
inline constexpr QLatin1String ConnectionRejected{"connection.rejected"};
inline constexpr QLatin1String ConnectionEnded{"connection.ended"};
inline constexpr QLatin1String ConnectionSignal{"connection.signal"};
inline constexpr QLatin1String Error{"error"};
} // namespace server

/** WebSocket close code sent by the server when authentication fails. */
inline constexpr int kCloseUnauthorized = 4401;
/** WebSocket close code sent when another socket took over this device. */
inline constexpr int kCloseReplaced = 4001;

struct DeviceSummary
{
    QString id;
    QString name;
    QString platform;

    static DeviceSummary fromJson(const QJsonObject &json)
    {
        return {json.value(QLatin1String("id")).toString(), json.value(QLatin1String("name")).toString(),
                json.value(QLatin1String("platform")).toString()};
    }
};

enum class Role { Host, Viewer };

} // namespace anypc::protocol
