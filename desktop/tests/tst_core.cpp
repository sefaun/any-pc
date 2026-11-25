#include "core/AppSettings.h"
#include "core/DeviceId.h"
#include "core/PasswordHasher.h"
#include "ui/QrCodeWidget.h"

#include <QtTest>

using namespace anypc;

class TestCore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void deviceIdNormalizeAndFormat()
    {
        QCOMPARE(DeviceId::normalize(QStringLiteral(" 482 913-507 ")), QStringLiteral("482913507"));
        QCOMPARE(DeviceId::format(QStringLiteral("482913507")), QStringLiteral("482 913 507"));
        QVERIFY(DeviceId::isValid(QStringLiteral("482913507")));
        QVERIFY(!DeviceId::isValid(QStringLiteral("082913507")));
        QVERIFY(!DeviceId::isValid(QStringLiteral("48291350")));
    }

    void deviceIdQrCode()
    {
        const QString payload = DeviceId::qrPayload(QStringLiteral("482913507"));
        QCOMPARE(payload, QStringLiteral("anypc://connect?id=482913507"));

        const QImage image = QrCodeWidget::render(payload);
        QVERIFY(!image.isNull());
        // 27 bytes need a version 3 code at medium error correction (29 modules) plus a 4 module quiet zone.
        QCOMPARE(image.width(), 29 + 8);
        QCOMPARE(image.height(), image.width());
        // Top-left finder pattern: dark corner inside the quiet zone, light quiet zone around it.
        QCOMPARE(image.pixelIndex(4, 4), 1);
        QCOMPARE(image.pixelIndex(0, 0), 0);
        QVERIFY(QrCodeWidget::render(QString()).isNull());
    }

    void serverUrlNormalization_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");
        QTest::newRow("bare host") << "anypc.example.com" << "https://anypc.example.com";
        QTest::newRow("trailing slash") << "https://anypc.example.com/" << "https://anypc.example.com";
        QTest::newRow("http with port") << "http://localhost:3000" << "http://localhost:3000";
        QTest::newRow("query dropped") << "https://a.example.com/?x=1#f" << "https://a.example.com";
        QTest::newRow("base path kept") << "https://example.com/anypc/" << "https://example.com/anypc";
        QTest::newRow("empty") << "   " << "";
        QTest::newRow("ftp rejected") << "ftp://example.com" << "";
    }

    void serverUrlNormalization()
    {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        QCOMPARE(AppSettings::normalizeServerUrl(input).toString(), expected);
    }

    void passwordHashing()
    {
        const QString hash = PasswordHasher::hash(QStringLiteral("secret123"));
        QVERIFY(hash.startsWith(QLatin1String("pbkdf2-sha256:")));
        QVERIFY(PasswordHasher::verify(QStringLiteral("secret123"), hash));
        QVERIFY(!PasswordHasher::verify(QStringLiteral("secret124"), hash));
        QVERIFY(!PasswordHasher::verify(QStringLiteral("secret123"), QStringLiteral("garbage")));
        QVERIFY(hash != PasswordHasher::hash(QStringLiteral("secret123"))); // salted
    }
};

QTEST_GUILESS_MAIN(TestCore)
#include "tst_core.moc"
