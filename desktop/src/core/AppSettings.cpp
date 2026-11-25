#include "core/AppSettings.h"

#include "BuildInfo.h"
#include "core/PasswordHasher.h"
#include "core/RecentConnections.h"

#include <QSysInfo>

namespace anypc {

namespace {
namespace Key {
constexpr auto ServerUrl = "network/serverUrl";
constexpr auto DeviceName = "device/name";
constexpr auto DeviceId = "device/id";
constexpr auto DeviceSecret = "device/secret";
constexpr auto UnattendedHash = "security/unattendedPasswordHash";
constexpr auto MinimizeToTray = "ui/minimizeToTray";
constexpr auto ClipboardSync = "session/clipboardSync";
} // namespace Key

} // namespace

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
    , m_recent(new RecentConnections(&m_settings, this))
{
}

bool AppSettings::hasServerUrl() const
{
    return serverUrl().isValid();
}

QUrl AppSettings::serverUrl() const
{
    const QUrl url(m_settings.value(Key::ServerUrl).toString());
    const bool usable = url.isValid() && !url.host().isEmpty()
                        && (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https"));
    return usable ? url : QUrl();
}

void AppSettings::setServerUrl(const QUrl &url)
{
    const QUrl cleaned = normalizeServerUrl(url.toString());
    const QUrl normalized = cleaned.isValid() ? cleaned : url;
    if (m_settings.contains(Key::ServerUrl) && normalized == serverUrl())
        return;
    m_settings.setValue(Key::ServerUrl, normalized.toString());
    m_settings.sync();
    Q_EMIT serverUrlChanged(normalized);
}

QUrl AppSettings::suggestedServerUrl()
{
    return normalizeServerUrl(QString::fromLatin1(build::kDefaultServerUrl));
}

QUrl AppSettings::normalizeServerUrl(const QString &input)
{
    QString text = input.trimmed();
    if (text.isEmpty())
        return {};
    if (!text.contains(QLatin1String("://")))
        text.prepend(QLatin1String("https://"));
    QUrl url(text, QUrl::StrictMode);
    if (!url.isValid() || url.host().isEmpty()
        || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https")))
        return {};
    // Only scheme, host, port and an optional base path are meaningful.
    url.setQuery(QString());
    url.setFragment(QString());
    QString path = url.path();
    while (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    url.setPath(path);
    return url;
}

QString AppSettings::deviceName() const
{
    const QString name = m_settings.value(Key::DeviceName).toString().trimmed();
    if (!name.isEmpty())
        return name;
    const QString host = QSysInfo::machineHostName().trimmed();
    return host.isEmpty() ? QStringLiteral("AnyPC Device") : host.left(64);
}

void AppSettings::setDeviceName(const QString &name)
{
    const QString trimmed = name.trimmed().left(64);
    if (trimmed == deviceName())
        return;
    m_settings.setValue(Key::DeviceName, trimmed);
    Q_EMIT deviceNameChanged(deviceName());
}

QString AppSettings::deviceId() const
{
    return m_settings.value(Key::DeviceId).toString();
}

QString AppSettings::deviceSecret() const
{
    return m_settings.value(Key::DeviceSecret).toString();
}

void AppSettings::setDeviceCredentials(const QString &deviceId, const QString &deviceSecret)
{
    m_settings.setValue(Key::DeviceId, deviceId);
    m_settings.setValue(Key::DeviceSecret, deviceSecret);
    m_settings.sync();
}

void AppSettings::clearDeviceCredentials()
{
    m_settings.remove(Key::DeviceId);
    m_settings.remove(Key::DeviceSecret);
}

bool AppSettings::hasUnattendedPassword() const
{
    return !m_settings.value(Key::UnattendedHash).toString().isEmpty();
}

void AppSettings::setUnattendedPassword(const QString &password)
{
    m_settings.setValue(Key::UnattendedHash, PasswordHasher::hash(password));
    Q_EMIT unattendedAccessChanged(true);
}

void AppSettings::clearUnattendedPassword()
{
    m_settings.remove(Key::UnattendedHash);
    Q_EMIT unattendedAccessChanged(false);
}

bool AppSettings::verifyUnattendedPassword(const QString &password) const
{
    const QString encoded = m_settings.value(Key::UnattendedHash).toString();
    return !encoded.isEmpty() && !password.isEmpty() && PasswordHasher::verify(password, encoded);
}

bool AppSettings::minimizeToTray() const
{
    return m_settings.value(Key::MinimizeToTray, true).toBool();
}

void AppSettings::setMinimizeToTray(bool enabled)
{
    m_settings.setValue(Key::MinimizeToTray, enabled);
}

bool AppSettings::clipboardSyncEnabled() const
{
    return m_settings.value(Key::ClipboardSync, true).toBool();
}

void AppSettings::setClipboardSyncEnabled(bool enabled)
{
    m_settings.setValue(Key::ClipboardSync, enabled);
}


} // namespace anypc
