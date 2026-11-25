#include "core/RecentConnections.h"

#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace anypc;

class TestRecent : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString path() const { return m_dir.filePath(QStringLiteral("settings.ini")); }

private Q_SLOTS:
    void init() { QFile::remove(path()); }

    void recordsMostRecentFirstAndCounts()
    {
        QSettings settings(path(), QSettings::IniFormat);
        RecentConnections recent(&settings);
        QSignalSpy changed(&recent, &RecentConnections::changed);

        recent.recordAttempt(QStringLiteral("111111111"));
        recent.recordAttempt(QStringLiteral("222222222"));
        recent.recordAttempt(QStringLiteral("111111111"));
        recent.recordAttempt(QStringLiteral("012345678")); // invalid id, ignored

        QCOMPARE(recent.entries().size(), 2);
        QCOMPARE(recent.entries().at(0).id, QStringLiteral("111111111"));
        QCOMPARE(recent.entries().at(0).connectionCount, 2);
        QVERIFY(recent.entries().at(0).lastConnected.isValid());
        QCOMPARE(changed.count(), 3);
    }

    void persistsDetailsAliasAndRemoval()
    {
        {
            QSettings settings(path(), QSettings::IniFormat);
            RecentConnections recent(&settings);
            recent.recordAttempt(QStringLiteral("333333333"));
            recent.updateDetails(QStringLiteral("333333333"), QStringLiteral("Office PC"), QStringLiteral("windows"));
            recent.recordAttempt(QStringLiteral("444444444"));
            recent.rename(QStringLiteral("444444444"), QStringLiteral("  Home Mac  "));
        }
        QSettings settings(path(), QSettings::IniFormat);
        RecentConnections reloaded(&settings);
        QCOMPARE(reloaded.entries().size(), 2);
        const auto *office = reloaded.find(QStringLiteral("333333333"));
        QVERIFY(office);
        QCOMPARE(office->displayName(), QStringLiteral("Office PC"));
        QCOMPARE(office->platform, QStringLiteral("windows"));
        QCOMPARE(reloaded.find(QStringLiteral("444444444"))->displayName(), QStringLiteral("Home Mac"));

        reloaded.remove(QStringLiteral("333333333"));
        QVERIFY(!reloaded.find(QStringLiteral("333333333")));
        reloaded.clear();
        QVERIFY(reloaded.entries().isEmpty());
    }

    void displayNameFallsBackToFormattedId()
    {
        RecentConnections::Entry e;
        e.id = QStringLiteral("555666777");
        QCOMPARE(e.displayName(), QStringLiteral("555 666 777"));
    }

    void capsTheHistory()
    {
        QSettings settings(path(), QSettings::IniFormat);
        RecentConnections recent(&settings);
        for (int i = 0; i < RecentConnections::kMaxEntries + 5; ++i)
            recent.recordAttempt(QString::number(100000000 + i));
        QCOMPARE(recent.entries().size(), RecentConnections::kMaxEntries);
        QCOMPARE(recent.entries().first().id, QString::number(100000000 + RecentConnections::kMaxEntries + 4));
    }

    void migratesTheLegacyIdList()
    {
        {
            QSettings settings(path(), QSettings::IniFormat);
            settings.setValue(QStringLiteral("ui/recentConnections"),
                              QStringList{QStringLiteral("123456789"), QStringLiteral("987654321")});
        }
        QSettings settings(path(), QSettings::IniFormat);
        RecentConnections recent(&settings);
        QCOMPARE(recent.entries().size(), 2);
        QCOMPARE(recent.entries().at(0).id, QStringLiteral("123456789"));
        QVERIFY(!settings.contains(QStringLiteral("ui/recentConnections")));
    }
};

QTEST_GUILESS_MAIN(TestRecent)
#include "tst_recent.moc"
