#pragma once

#include <QDateTime>
#include <QFile>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

namespace anypc {

/**
 * File transfer bookkeeping for one session (both directions).
 *
 * The embedded page moves the bytes over the "files" data channel; this
 * class owns all file system access. Outgoing files are sent one at a time
 * (pull model: the page asks for the next chunk when the channel can take it),
 * incoming files are written to a ".part" file and renamed when complete.
 */
class FileTransferManager final : public QObject
{
    Q_OBJECT

public:
    enum class Direction { Outgoing, Incoming };
    enum class State { Queued, Transferring, Completed, Failed };

    struct Transfer
    {
        QString id;
        Direction direction = Direction::Outgoing;
        QString name;       ///< Display/file name (sanitised for incoming files).
        QString path;       ///< Local source or destination path.
        qint64 size = 0;
        qint64 transferred = 0;
        State state = State::Queued;
        QString error;
    };

    /** Raw bytes per data channel message (safe for every WebRTC stack). */
    static constexpr int kChunkSize = 16 * 1024;

    explicit FileTransferManager(const QString &downloadDirectory, QObject *parent = nullptr);
    ~FileTransferManager() override;

    QString downloadDirectory() const { return m_downloadDirectory; }
    const QList<Transfer> &transfers() const { return m_transfers; }
    const Transfer *find(const QString &id) const;

    /** Queues local files for sending. Unreadable paths are reported as failed. */
    void sendFiles(const QStringList &paths);
    /** The page reports whether the files channel is open. */
    void setChannelReady(bool ready);
    /** Fails every unfinished transfer (e.g. session ended). */
    void abortAll(const QString &reason);

    // ---- Outgoing (driven by the page) --------------------------------------------
    /** Next chunk of an outgoing file as base64; empty at end of file or on error. */
    QString readChunk(const QString &id);
    void finishOutgoing(const QString &id, bool ok, const QString &error);

    // ---- Incoming (driven by the page) --------------------------------------------
    bool beginIncoming(const QString &id, const QString &name, qint64 size);
    bool writeChunk(const QString &id, const QString &base64);
    bool finishIncoming(const QString &id);
    void failIncoming(const QString &id, const QString &error);

    /** Strips directories and characters that are invalid on common file systems. */
    static QString sanitizeFileName(const QString &name);
    /** A path in `directory` named `name` that does not exist yet ("a (1).txt", ...). */
    static QString uniquePath(const QString &directory, const QString &name);

Q_SIGNALS:
    /** Ask the page to announce and stream an outgoing file. */
    void offerFile(const QString &id, const QString &name, double size);
    void transferAdded(const QString &id);
    void transferUpdated(const QString &id);

private:
    Transfer *findMutable(const QString &id);
    void startNext();
    void fail(Transfer &transfer, const QString &error);
    void closeActive();

    QString m_downloadDirectory;
    QList<Transfer> m_transfers;
    bool m_channelReady = false;

    QString m_activeOutgoing;
    std::unique_ptr<QFile> m_outgoingFile;
    QString m_activeIncoming;
    std::unique_ptr<QFile> m_incomingFile;
};

} // namespace anypc
