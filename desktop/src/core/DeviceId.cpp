#include "core/DeviceId.h"

#include <QRegularExpression>

namespace anypc::DeviceId {

QString normalize(const QString &input)
{
    QString digits;
    digits.reserve(input.size());
    for (const QChar ch : input) {
        if (ch.isDigit())
            digits.append(ch);
    }
    return digits;
}

bool isValid(const QString &normalized)
{
    static const QRegularExpression pattern(QStringLiteral("^[1-9]\\d{8}$"));
    return pattern.match(normalized).hasMatch();
}

QString format(const QString &normalized)
{
    if (normalized.size() != 9)
        return normalized;
    return normalized.mid(0, 3) + QLatin1Char(' ') + normalized.mid(3, 3) + QLatin1Char(' ')
           + normalized.mid(6, 3);
}

QString qrPayload(const QString &normalized)
{
    return QStringLiteral("anypc://connect?id=") + normalized;
}

} // namespace anypc::DeviceId
