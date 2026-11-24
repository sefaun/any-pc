#include "core/PasswordHasher.h"

#include <QCryptographicHash>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QStringList>

namespace anypc {

namespace {
constexpr auto kScheme = "pbkdf2-sha256";
constexpr int kIterations = 120000;
constexpr int kKeyLength = 32;
constexpr int kSaltLength = 16;

QByteArray derive(const QString &password, const QByteArray &salt, int iterations)
{
    return QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256, password.toUtf8(), salt,
                                              iterations, kKeyLength);
}

bool constantTimeEquals(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size())
        return false;
    unsigned char diff = 0;
    for (int i = 0; i < a.size(); ++i)
        diff |= static_cast<unsigned char>(a.at(i) ^ b.at(i));
    return diff == 0;
}
} // namespace

QString PasswordHasher::hash(const QString &password)
{
    QByteArray salt(kSaltLength, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(salt.data()), kSaltLength / 4);
    const QByteArray key = derive(password, salt, kIterations);
    return QStringList{QString::fromLatin1(kScheme), QString::number(kIterations),
                       QString::fromLatin1(salt.toBase64()), QString::fromLatin1(key.toBase64())}
        .join(QLatin1Char(':'));
}

bool PasswordHasher::verify(const QString &password, const QString &encoded)
{
    const QStringList parts = encoded.split(QLatin1Char(':'));
    if (parts.size() != 4 || parts.at(0) != QLatin1String(kScheme))
        return false;
    bool ok = false;
    const int iterations = parts.at(1).toInt(&ok);
    if (!ok || iterations <= 0)
        return false;
    const QByteArray salt = QByteArray::fromBase64(parts.at(2).toLatin1());
    const QByteArray expected = QByteArray::fromBase64(parts.at(3).toLatin1());
    return constantTimeEquals(derive(password, salt, iterations), expected);
}

} // namespace anypc
