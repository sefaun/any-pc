#pragma once

#include <QDialog>
#include <QHash>
#include <QPointer>

class QTreeWidget;
class QTreeWidgetItem;
class QProgressBar;
class QLabel;

namespace anypc {

class FileTransferManager;

/** Non-modal list of a session's file transfers with progress. */
class FileTransferDialog final : public QDialog
{
    Q_OBJECT

public:
    FileTransferDialog(FileTransferManager *transfers, const QString &peerName, QWidget *parent = nullptr);

Q_SIGNALS:
    void sendFilesRequested();

private:
    void addRow(const QString &id);
    void updateRow(const QString &id);

    QPointer<FileTransferManager> m_transfers;
    QTreeWidget *m_list;
    QLabel *m_summary;
    QHash<QString, QTreeWidgetItem *> m_rows;
    QHash<QString, QProgressBar *> m_bars;
};

} // namespace anypc
