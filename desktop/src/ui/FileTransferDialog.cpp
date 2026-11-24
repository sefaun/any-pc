#include "ui/FileTransferDialog.h"

#include "transfer/FileTransferManager.h"

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace anypc {

namespace {
enum Column { DirectionColumn, NameColumn, ProgressColumn, StatusColumn };
} // namespace

FileTransferDialog::FileTransferDialog(FileTransferManager *transfers, const QString &peerName, QWidget *parent)
    : QDialog(parent)
    , m_transfers(transfers)
    , m_list(new QTreeWidget(this))
    , m_summary(new QLabel(this))
{
    setWindowTitle(tr("File transfer — %1").arg(peerName));
    setMinimumSize(620, 300);

    m_list->setColumnCount(4);
    m_list->setHeaderLabels({QString(), tr("File"), tr("Progress"), tr("Status")});
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->header()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
    m_list->header()->setSectionResizeMode(DirectionColumn, QHeaderView::ResizeToContents);
    m_list->setColumnWidth(ProgressColumn, 160);
    m_list->setColumnWidth(StatusColumn, 170);

    m_summary->setObjectName(QStringLiteral("cardHint"));
    m_summary->setText(tr("Received files are saved to %1").arg(QDir::toNativeSeparators(transfers->downloadDirectory())));
    m_summary->setWordWrap(true);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    QPushButton *send = buttons->addButton(tr("Send files…"), QDialogButtonBox::ActionRole);
    send->setObjectName(QStringLiteral("primary"));
    QPushButton *open = buttons->addButton(tr("Open folder"), QDialogButtonBox::ActionRole);
    connect(send, &QPushButton::clicked, this, &FileTransferDialog::sendFilesRequested);
    connect(open, &QPushButton::clicked, this, [this]() {
        if (!m_transfers)
            return;
        QDir().mkpath(m_transfers->downloadDirectory());
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_transfers->downloadDirectory()));
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::hide);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_list);
    layout->addWidget(m_summary);
    layout->addWidget(buttons);

    for (const auto &t : transfers->transfers())
        addRow(t.id);
    connect(transfers, &FileTransferManager::transferAdded, this, &FileTransferDialog::addRow);
    connect(transfers, &FileTransferManager::transferUpdated, this, &FileTransferDialog::updateRow);
}

void FileTransferDialog::addRow(const QString &id)
{
    if (m_rows.contains(id) || !m_transfers || !m_transfers->find(id))
        return;
    auto *item = new QTreeWidgetItem(m_list);
    auto *bar = new QProgressBar(m_list);
    bar->setRange(0, 1000);
    bar->setTextVisible(true);
    m_list->setItemWidget(item, ProgressColumn, bar);
    m_rows.insert(id, item);
    m_bars.insert(id, bar);
    updateRow(id);
    m_list->scrollToItem(item);
}

void FileTransferDialog::updateRow(const QString &id)
{
    QTreeWidgetItem *item = m_rows.value(id);
    QProgressBar *bar = m_bars.value(id);
    const FileTransferManager::Transfer *t = m_transfers ? m_transfers->find(id) : nullptr;
    if (!item || !bar || !t)
        return;

    const bool outgoing = t->direction == FileTransferManager::Direction::Outgoing;
    item->setText(DirectionColumn, outgoing ? QStringLiteral("↑") : QStringLiteral("↓"));
    item->setToolTip(DirectionColumn, outgoing ? tr("Sent") : tr("Received"));
    item->setText(NameColumn, t->name);
    item->setToolTip(NameColumn, QDir::toNativeSeparators(t->path));

    const int permille = t->size > 0 ? int(t->transferred * 1000 / t->size) : (t->state == FileTransferManager::State::Completed ? 1000 : 0);
    bar->setValue(permille);
    bar->setFormat(QStringLiteral("%1 / %2").arg(QLocale().formattedDataSize(t->transferred), QLocale().formattedDataSize(t->size)));

    QString status;
    switch (t->state) {
    case FileTransferManager::State::Queued: status = tr("Waiting"); break;
    case FileTransferManager::State::Transferring: status = outgoing ? tr("Sending…") : tr("Receiving…"); break;
    case FileTransferManager::State::Completed: status = outgoing ? tr("Sent") : tr("Saved"); break;
    case FileTransferManager::State::Failed: status = tr("Failed: %1").arg(t->error); break;
    }
    item->setText(StatusColumn, status);
    item->setToolTip(StatusColumn, status);
}

} // namespace anypc
