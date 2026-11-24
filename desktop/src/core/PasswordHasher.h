#pragma once

#include <QString>

namespace anypc {

/**
 * PBKDF2-HMAC-SHA256 password hashing for the local unattended-access password.
 * Encoded format: `pbkdf2-sha256:<iterations>:<salt b64>:<hash b64>`.
 */
class PasswordHasher final
{
public:
    static QString hash(const QString &password);
    static bool verify(const QString &password, const QString &encoded);

private:
    PasswordHasher() = delete;
};

} // namespace anypc
