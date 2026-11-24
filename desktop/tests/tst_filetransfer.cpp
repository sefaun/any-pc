#include "transfer/FileTransferManager.h"

#include <QFile>
#include <QRandomGenerator>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace anypc;

class TestFileTransfer : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void sanitizesNames_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");
        QTest::newRow("plain") << "report.pdf" << "report.pdf";
        QTest::newRow("unix traversal") << "../../etc/passwd" << "passwd";
        QTest::newRow("windows path") << "C:\\\\Users\\\\x\\\\notes.txt" << "notes.txt";
        QTest::newRow("hidden") << ".bashrc" << "bashrc";
        QTest::newRow("reserved chars") << "a<b>:c?.txt" << "a_b__c_.txt";
        QTest::newRow("empty") << "..." << "file";
    }

    void sanitizesNames()
    {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        QCOMPARE(FileTransferManager::sanitizeFileName(input), expected);
    }

    void uniquePathAvoidsOverwriting()
    {
        QTemporaryDir dir;
        QFile existing(dir.filePath(QStringLiteral("a.txt")));
        QVERIFY(existing.open(QIODevice::WriteOnly));
        existing.close();
        QCOMPARE(FileTransferManager::uniquePath(dir.path(), QStringLiteral("a.txt")), dir.filePath(QStringLiteral("a (1).txt")));
        QCOMPARE(FileTransferManager::uniquePath(dir.path(), QStringLiteral("b.txt")), dir.filePath(QStringLiteral("b.txt")));
    }

    /** Sender and receiver wired back to back, like two pages over a data channel. */
    void loopbackTransferPreservesContent()
    {
        QTemporaryDir sourceDir;
        QTemporaryDir targetDir;
        QByteArray content(FileTransferManager::kChunkSize * 3 + 123, Qt::Uninitialized);
        for (char &c : content)
            c = char(QRandomGenerator::global()->bounded(256));
        const QString sourcePath = sourceDir.filePath(QStringLiteral("data.bin"));
        QFile source(sourcePath);
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write(content);
        source.close();

        FileTransferManager sender(sourceDir.filePath(QStringLiteral("unused")));
        FileTransferManager receiver(targetDir.path());
        QSignalSpy offers(&sender, &FileTransferManager::offerFile);

        sender.sendFiles({sourcePath});
        QCOMPARE(offers.count(), 0); // nothing moves until the channel is open
        sender.setChannelReady(true);
        QCOMPARE(offers.count(), 1);

        const QString id = offers.at(0).at(0).toString();
        QVERIFY(receiver.beginIncoming(id, offers.at(0).at(1).toString(), qint64(offers.at(0).at(2).toDouble())));
        int chunks = 0;
        for (QString chunk = sender.readChunk(id); !chunk.isEmpty(); chunk = sender.readChunk(id)) {
            QVERIFY(receiver.writeChunk(id, chunk));
            ++chunks;
        }
        QCOMPARE(chunks, 4);
        QVERIFY(receiver.finishIncoming(id));
        sender.finishOutgoing(id, true, QString());

        QCOMPARE(sender.find(id)->state, FileTransferManager::State::Completed);
        QCOMPARE(receiver.find(id)->state, FileTransferManager::State::Completed);
        QFile saved(targetDir.filePath(QStringLiteral("data.bin")));
        QVERIFY(saved.open(QIODevice::ReadOnly));
        QCOMPARE(saved.readAll(), content);
    }

    void queuesFilesOneAtATime()
    {
        QTemporaryDir dir;
        QStringList paths;
        for (const char *name : {"one.txt", "two.txt"}) {
            QFile f(dir.filePath(QString::fromLatin1(name)));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("hello");
            paths << f.fileName();
        }
        FileTransferManager sender(dir.path());
        QSignalSpy offers(&sender, &FileTransferManager::offerFile);
        sender.setChannelReady(true);
        sender.sendFiles(paths);
        QCOMPARE(offers.count(), 1);
        const QString first = offers.at(0).at(0).toString();
        while (!sender.readChunk(first).isEmpty()) {
        }
        sender.finishOutgoing(first, true, QString());
        QCOMPARE(offers.count(), 2);
        QCOMPARE(offers.at(1).at(1).toString(), QStringLiteral("two.txt"));
    }

    void incompleteIncomingFileIsDiscarded()
    {
        QTemporaryDir dir;
        FileTransferManager receiver(dir.path());
        QVERIFY(receiver.beginIncoming(QStringLiteral("x"), QStringLiteral("big.bin"), 100));
        QVERIFY(receiver.writeChunk(QStringLiteral("x"), QString::fromLatin1(QByteArray(10, 'a').toBase64())));
        QVERIFY(!receiver.finishIncoming(QStringLiteral("x")));
        QCOMPARE(receiver.find(QStringLiteral("x"))->state, FileTransferManager::State::Failed);
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files).size(), 0);
    }

    void rejectsMoreDataThanAnnounced()
    {
        QTemporaryDir dir;
        FileTransferManager receiver(dir.path());
        QVERIFY(receiver.beginIncoming(QStringLiteral("y"), QStringLiteral("s.bin"), 4));
        QVERIFY(!receiver.writeChunk(QStringLiteral("y"), QString::fromLatin1(QByteArray(8, 'a').toBase64())));
        QCOMPARE(receiver.find(QStringLiteral("y"))->state, FileTransferManager::State::Failed);
    }

    void channelLossFailsActiveTransfer()
    {
        QTemporaryDir dir;
        QFile f(dir.filePath(QStringLiteral("f.txt")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("data");
        f.close();
        FileTransferManager sender(dir.path());
        QSignalSpy offers(&sender, &FileTransferManager::offerFile);
        sender.setChannelReady(true);
        sender.sendFiles({f.fileName()});
        const QString id = offers.at(0).at(0).toString();
        sender.setChannelReady(false);
        QCOMPARE(sender.find(id)->state, FileTransferManager::State::Failed);
    }
};

QTEST_GUILESS_MAIN(TestFileTransfer)
#include "tst_filetransfer.moc"
