#include "core/RecentConnections.h"

#include "core/DeviceId.h"

#include <QSettings>

namespace anypc {

namespace {
constexpr auto kGroup = "recentConnections";
constexpr auto kLegacyKey = "ui/recentConnections"; // ids only, used up to 0.2.0
} // namespace

QString RecentConnections::Entry::displayName() const
{
    if (!alias.isEmpty())
        return alias;
    if (!name.isEmpty())
        return name;
    return DeviceId::format(id);
}

RecentConnections::RecentConnections(QSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    load();
}

const RecentConnections::Entry *RecentConnections::find(const QString &id) const
{
    for (const Entry &e : m_entries) {
        if (e.id == id)
            return &e;
    }
    return nullptr;
}

RecentConnections::Entry *RecentConnections::findMutable(const QString &id)
{
    return const_cast<Entry *>(find(id));
}

void RecentConnections::load()
{
    m_entries.clear();
    const int count = m_settings->beginReadArray(QString::fromLatin1(kGroup));
    for (int i = 0; i < count; ++i) {
        m_settings->setArrayIndex(i);
        Entry e;
        e.id = m_settings->value(QStringLiteral("id")).toString();
        if (!DeviceId::isValid(e.id) || find(e.id))
            continue;
        e.name = m_settings->value(QStringLiteral("name")).toString();
        e.platform = m_settings->value(QStringLiteral("platform")).toString();
        e.alias = m_settings->value(QStringLiteral("alias")).toString();
        e.lastConnected = m_settings->value(QStringLiteral("lastConnected")).toDateTime();
        e.connectionCount = m_settings->value(QStringLiteral("count")).toInt();
        m_entries.append(e);
    }
    m_settings->endArray();

    // One-time migration of the old id-only list.
    if (m_settings->contains(QString::fromLatin1(kLegacyKey))) {
        for (const QString &id : m_settings->value(QString::fromLatin1(kLegacyKey)).toStringList()) {
            if (DeviceId::isValid(id) && !find(id)) {
                Entry e;
                e.id = id;
                m_entries.append(e);
            }
        }
        m_settings->remove(QString::fromLatin1(kLegacyKey));
        save();
    }
}

void RecentConnections::save()
{
    m_settings->remove(QString::fromLatin1(kGroup));
    m_settings->beginWriteArray(QString::fromLatin1(kGroup), int(m_entries.size()));
    for (int i = 0; i < m_entries.size(); ++i) {
        const Entry &e = m_entries.at(i);
        m_settings->setArrayIndex(i);
        m_settings->setValue(QStringLiteral("id"), e.id);
        m_settings->setValue(QStringLiteral("name"), e.name);
        m_settings->setValue(QStringLiteral("platform"), e.platform);
        m_settings->setValue(QStringLiteral("alias"), e.alias);
        m_settings->setValue(QStringLiteral("lastConnected"), e.lastConnected);
        m_settings->setValue(QStringLiteral("count"), e.connectionCount);
    }
    m_settings->endArray();
    m_settings->sync();
    Q_EMIT changed();
}

void RecentConnections::recordAttempt(const QString &id)
{
    if (!DeviceId::isValid(id))
        return;
    Entry entry;
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).id == id) {
            entry = m_entries.takeAt(i);
            break;
        }
    }
    entry.id = id;
    entry.lastConnected = QDateTime::currentDateTime();
    ++entry.connectionCount;
    m_entries.prepend(entry);
    while (m_entries.size() > kMaxEntries)
        m_entries.removeLast();
    save();
}

void RecentConnections::updateDetails(const QString &id, const QString &name, const QString &platform)
{
    Entry *e = findMutable(id);
    if (!e || (e->name == name && e->platform == platform))
        return;
    if (!name.isEmpty())
        e->name = name;
    if (!platform.isEmpty())
        e->platform = platform;
    save();
}

void RecentConnections::rename(const QString &id, const QString &alias)
{
    Entry *e = findMutable(id);
    const QString trimmed = alias.trimmed().left(64);
    if (!e || e->alias == trimmed)
        return;
    e->alias = trimmed;
    save();
}

void RecentConnections::remove(const QString &id)
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).id == id) {
            m_entries.removeAt(i);
            save();
            return;
        }
    }
}

void RecentConnections::clear()
{
    if (m_entries.isEmpty())
        return;
    m_entries.clear();
    save();
}

} // namespace anypc
