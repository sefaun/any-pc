#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace anypc {

/**
 * Persistent user and device configuration backed by QSettings.
 * Emits change signals so dependent services can react (e.g. reconnect
 * when the server URL changes).
 */
class AppSettings final : public QObject
{
    Q_OBJECT

public:
    explicit AppSettings(QObject *parent = nullptr);

    QUrl serverUrl() const;
    void setServerUrl(const QUrl &url);

    QString deviceName() const;
    void setDeviceName(const QString &name);

    QString deviceId() const;
    QString deviceSecret() const;
    void setDeviceCredentials(const QString &deviceId, const QString &deviceSecret);
    void clearDeviceCredentials();

    bool hasUnattendedPassword() const;
    void setUnattendedPassword(const QString &password);
    void clearUnattendedPassword();
    bool verifyUnattendedPassword(const QString &password) const;

    bool minimizeToTray() const;
    void setMinimizeToTray(bool enabled);

    bool clipboardSyncEnabled() const;
    void setClipboardSyncEnabled(bool enabled);

    QStringList recentConnections() const;
    void addRecentConnection(const QString &deviceId);

Q_SIGNALS:
    void serverUrlChanged(const QUrl &url);
    void deviceNameChanged(const QString &name);
    void unattendedAccessChanged(bool enabled);

private:
    QSettings m_settings;
};

} // namespace anypc
