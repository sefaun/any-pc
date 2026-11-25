#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>

class QSettings;

namespace anypc {

/**
 * Locally stored history of devices this computer connected to (most recent
 * first). Nothing is sent to the server. Passwords are never stored.
 */
class RecentConnections final : public QObject
{
    Q_OBJECT

public:
    struct Entry
    {
        QString id;            ///< Canonical 9 digit device id.
        QString name;          ///< Device name reported by the server.
        QString platform;      ///< "windows", "macos", "linux" or empty.
        QString alias;         ///< Optional user-chosen label.
        QDateTime lastConnected;
        int connectionCount = 0;

        /** Alias if set, otherwise the device name, otherwise the formatted id. */
        QString displayName() const;
    };

    static constexpr int kMaxEntries = 30;

    /** `settings` must outlive this object. */
    explicit RecentConnections(QSettings *settings, QObject *parent = nullptr);

    const QList<Entry> &entries() const { return m_entries; }
    const Entry *find(const QString &id) const;

    /** Records a connection attempt (moves the device to the top). */
    void recordAttempt(const QString &id);
    /** Updates name/platform once the server told us who the device is. */
    void updateDetails(const QString &id, const QString &name, const QString &platform);
    void rename(const QString &id, const QString &alias);
    void remove(const QString &id);
    void clear();

Q_SIGNALS:
    void changed();

private:
    void load();
    void save();
    Entry *findMutable(const QString &id);

    QSettings *m_settings;
    QList<Entry> m_entries;
};

} // namespace anypc
