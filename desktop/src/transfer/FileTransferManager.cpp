#include "transfer/FileTransferManager.h"

#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QUuid>

Q_LOGGING_CATEGORY(lcTransfer, "anypc.transfer")

namespace anypc {

namespace {
const QString kPartSuffix = QStringLiteral(".anypc-part");
constexpr int kMaxNameLength = 200;
} // namespace

FileTransferManager::FileTransferManager(const QString &downloadDirectory, QObject *parent)
    : QObject(parent)
    , m_downloadDirectory(downloadDirectory)
{
}

FileTransferManager::~FileTransferManager()
{
    abortAll(QStringLiteral("Session closed"));
}

const FileTransferManager::Transfer *FileTransferManager::find(const QString &id) const
{
    for (const Transfer &t : m_transfers) {
        if (t.id == id)
            return &t;
    }
    return nullptr;
}

FileTransferManager::Transfer *FileTransferManager::findMutable(const QString &id)
{
    return const_cast<Transfer *>(find(id));
}

QString FileTransferManager::sanitizeFileName(const QString &name)
{
    // Keep only the last path component, whatever separator the peer used.
    QString base = name;
    base.replace(QLatin1Char('\\'), QLatin1Char('/'));
    base = base.section(QLatin1Char('/'), -1);

    QString clean;
    clean.reserve(base.size());
    for (const QChar ch : std::as_const(base)) {
        static const QString forbidden = QStringLiteral("<>:\"|?*");
        clean.append(ch.unicode() < 32 || forbidden.contains(ch) ? QLatin1Char('_') : ch);
    }
    clean = clean.trimmed();
    while (clean.endsWith(QLatin1Char('.')) || clean.endsWith(QLatin1Char(' ')))
        clean.chop(1);
    while (clean.startsWith(QLatin1Char('.')))
        clean.remove(0, 1);
    if (clean.isEmpty())
        clean = QStringLiteral("file");
    if (clean.size() > kMaxNameLength) {
        const QString suffix = QFileInfo(clean).suffix().left(16);
        clean = clean.left(kMaxNameLength - suffix.size() - 1) + (suffix.isEmpty() ? QString() : QLatin1Char('.') + suffix);
    }
    return clean;
}

QString FileTransferManager::uniquePath(const QString &directory, const QString &name)
{
    const QDir dir(directory);
    const QFileInfo info(name);
    const QString stem = info.completeBaseName();
    const QString suffix = info.suffix().isEmpty() ? QString() : QLatin1Char('.') + info.suffix();
    QString candidate = dir.filePath(name);
    for (int i = 1; QFileInfo::exists(candidate) || QFileInfo::exists(candidate + kPartSuffix); ++i)
        candidate = dir.filePath(QStringLiteral("%1 (%2)%3").arg(stem).arg(i).arg(suffix));
    return candidate;
}

// ---- Outgoing ---------------------------------------------------------------------

void FileTransferManager::sendFiles(const QStringList &paths)
{
    for (const QString &path : paths) {
        const QFileInfo info(path);
        Transfer t;
        t.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        t.direction = Direction::Outgoing;
        t.name = info.fileName();
        t.path = info.absoluteFilePath();
        t.size = info.size();
        if (!info.isFile() || !info.isReadable()) {
            t.state = State::Failed;
            t.error = tr("File cannot be read");
        }
        m_transfers.append(t);
        Q_EMIT transferAdded(t.id);
    }
    startNext();
}

void FileTransferManager::setChannelReady(bool ready)
{
    m_channelReady = ready;
    if (ready) {
        startNext();
        return;
    }
    // The channel closed: anything in flight cannot complete.
    if (Transfer *t = findMutable(m_activeOutgoing))
        fail(*t, tr("Connection closed"));
    if (Transfer *t = findMutable(m_activeIncoming))
        fail(*t, tr("Connection closed"));
}

void FileTransferManager::startNext()
{
    if (!m_channelReady || !m_activeOutgoing.isEmpty())
        return;
    for (Transfer &t : m_transfers) {
        if (t.direction != Direction::Outgoing || t.state != State::Queued)
            continue;
        auto file = std::make_unique<QFile>(t.path);
        if (!file->open(QIODevice::ReadOnly)) {
            fail(t, file->errorString());
            continue;
        }
        m_outgoingFile = std::move(file);
        m_activeOutgoing = t.id;
        t.state = State::Transferring;
        t.size = m_outgoingFile->size();
        Q_EMIT transferUpdated(t.id);
        qCInfo(lcTransfer) << "Sending" << t.name << t.size << "bytes";
        Q_EMIT offerFile(t.id, t.name, double(t.size));
        return;
    }
}

QString FileTransferManager::readChunk(const QString &id)
{
    Transfer *t = findMutable(id);
    if (!t || id != m_activeOutgoing || !m_outgoingFile)
        return {};
    const QByteArray data = m_outgoingFile->read(kChunkSize);
    if (data.isEmpty())
        return {};
    t->transferred += data.size();
    Q_EMIT transferUpdated(id);
    return QString::fromLatin1(data.toBase64());
}

void FileTransferManager::finishOutgoing(const QString &id, bool ok, const QString &error)
{
    Transfer *t = findMutable(id);
    if (t && t->direction == Direction::Outgoing && t->state == State::Transferring) {
        if (ok && t->transferred == t->size) {
            t->state = State::Completed;
            qCInfo(lcTransfer) << "Sent" << t->name;
            Q_EMIT transferUpdated(id);
        } else {
            fail(*t, error.isEmpty() ? tr("Transfer failed") : error);
        }
    }
    if (id == m_activeOutgoing) {
        m_activeOutgoing.clear();
        m_outgoingFile.reset();
    }
    startNext();
}

// ---- Incoming ---------------------------------------------------------------------

bool FileTransferManager::beginIncoming(const QString &id, const QString &name, qint64 size)
{
    if (id.isEmpty() || find(id) || size < 0)
        return false;
    if (Transfer *previous = findMutable(m_activeIncoming))
        fail(*previous, tr("Interrupted by a new transfer"));

    Transfer t;
    t.id = id;
    t.direction = Direction::Incoming;
    t.name = sanitizeFileName(name);
    t.size = size;

    if (!QDir().mkpath(m_downloadDirectory)) {
        t.state = State::Failed;
        t.error = tr("Cannot create %1").arg(m_downloadDirectory);
        m_transfers.append(t);
        Q_EMIT transferAdded(id);
        return false;
    }
    t.path = uniquePath(m_downloadDirectory, t.name);
    auto file = std::make_unique<QFile>(t.path + kPartSuffix);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        t.state = State::Failed;
        t.error = file->errorString();
        m_transfers.append(t);
        Q_EMIT transferAdded(id);
        return false;
    }
    t.state = State::Transferring;
    m_transfers.append(t);
    m_incomingFile = std::move(file);
    m_activeIncoming = id;
    qCInfo(lcTransfer) << "Receiving" << t.name << size << "bytes";
    Q_EMIT transferAdded(id);
    return true;
}

bool FileTransferManager::writeChunk(const QString &id, const QString &base64)
{
    Transfer *t = findMutable(id);
    if (!t || id != m_activeIncoming || !m_incomingFile)
        return false;
    const QByteArray data = QByteArray::fromBase64(base64.toLatin1());
    if (t->transferred + data.size() > t->size) {
        fail(*t, tr("Received more data than announced"));
        return false;
    }
    if (m_incomingFile->write(data) != data.size()) {
        fail(*t, m_incomingFile->errorString());
        return false;
    }
    t->transferred += data.size();
    Q_EMIT transferUpdated(id);
    return true;
}

bool FileTransferManager::finishIncoming(const QString &id)
{
    Transfer *t = findMutable(id);
    if (!t || id != m_activeIncoming || !m_incomingFile)
        return false;
    if (t->transferred != t->size) {
        fail(*t, tr("Incomplete file (%1 of %2 bytes)").arg(t->transferred).arg(t->size));
        return false;
    }
    m_incomingFile->close();
    const QString partPath = m_incomingFile->fileName();
    m_incomingFile.reset();
    m_activeIncoming.clear();
    if (!QFile::rename(partPath, t->path)) {
        QFile::remove(partPath);
        t->state = State::Failed;
        t->error = tr("Cannot save %1").arg(t->path);
        Q_EMIT transferUpdated(id);
        return false;
    }
    t->state = State::Completed;
    qCInfo(lcTransfer) << "Received" << t->path;
    Q_EMIT transferUpdated(id);
    return true;
}

void FileTransferManager::failIncoming(const QString &id, const QString &error)
{
    if (Transfer *t = findMutable(id))
        fail(*t, error);
}

// ---- Common -----------------------------------------------------------------------

void FileTransferManager::fail(Transfer &transfer, const QString &error)
{
    if (transfer.state == State::Completed || transfer.state == State::Failed)
        return;
    transfer.state = State::Failed;
    transfer.error = error;
    qCWarning(lcTransfer) << "Transfer failed:" << transfer.name << error;

    if (transfer.id == m_activeOutgoing) {
        m_activeOutgoing.clear();
        m_outgoingFile.reset();
    }
    if (transfer.id == m_activeIncoming) {
        if (m_incomingFile) {
            m_incomingFile->close();
            m_incomingFile->remove();
            m_incomingFile.reset();
        }
        m_activeIncoming.clear();
    }
    Q_EMIT transferUpdated(transfer.id);
}

void FileTransferManager::abortAll(const QString &reason)
{
    for (Transfer &t : m_transfers) {
        if (t.state == State::Queued || t.state == State::Transferring)
            fail(t, reason);
    }
}

} // namespace anypc
