#include "ui/RecentConnectionsView.h"

#include "core/DeviceId.h"
#include "core/RecentConnections.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>

namespace anypc {

namespace {

constexpr int kIdRole = Qt::UserRole + 1;
const QSize kIconSize(72, 52);

QColor platformColor(const QString &platform)
{
    if (platform == QLatin1String("windows"))
        return QColor(0x00, 0x78, 0xd4);
    if (platform == QLatin1String("macos"))
        return QColor(0x63, 0x63, 0x6b);
    if (platform == QLatin1String("linux"))
        return QColor(0xe9, 0x54, 0x20);
    return QColor(0xe5, 0x48, 0x4d);
}

QString platformLabel(const QString &platform)
{
    if (platform == QLatin1String("windows"))
        return QStringLiteral("Windows");
    if (platform == QLatin1String("macos"))
        return QStringLiteral("macOS");
    if (platform == QLatin1String("linux"))
        return QStringLiteral("Linux");
    return QString();
}

/** A small monitor drawing tinted by platform, with the device initial. */
QIcon deviceIcon(const RecentConnections::Entry &entry, qreal dpr)
{
    QPixmap pixmap(kIconSize * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);

    const QColor color = platformColor(entry.platform);
    const QRectF screen(4, 2, kIconSize.width() - 8, kIconSize.height() - 14);
    QPainterPath frame;
    frame.addRoundedRect(screen, 6, 6);
    p.fillPath(frame, color);
    p.setPen(QPen(color.darker(120), 3, Qt::SolidLine, Qt::RoundCap));
    const qreal cx = kIconSize.width() / 2.0;
    p.drawLine(QPointF(cx, screen.bottom()), QPointF(cx, kIconSize.height() - 4));
    p.drawLine(QPointF(cx - 12, kIconSize.height() - 3), QPointF(cx + 12, kIconSize.height() - 3));

    QFont font = p.font();
    font.setBold(true);
    font.setPixelSize(18);
    p.setFont(font);
    p.setPen(Qt::white);
    const QString initial = entry.displayName().left(1).toUpper();
    p.drawText(screen, Qt::AlignCenter, initial);
    return QIcon(pixmap);
}

QString relativeTime(const QDateTime &when)
{
    if (!when.isValid())
        return RecentConnectionsView::tr("Never connected");
    const qint64 secs = when.secsTo(QDateTime::currentDateTime());
    if (secs < 60)
        return RecentConnectionsView::tr("Just now");
    if (secs < 3600)
        return RecentConnectionsView::tr("%n minute(s) ago", nullptr, int(secs / 60));
    if (secs < 86400)
        return RecentConnectionsView::tr("%n hour(s) ago", nullptr, int(secs / 3600));
    if (secs < 7 * 86400)
        return RecentConnectionsView::tr("%n day(s) ago", nullptr, int(secs / 86400));
    return QLocale().toString(when.date(), QLocale::ShortFormat);
}

} // namespace

RecentConnectionsView::RecentConnectionsView(RecentConnections *recent, QWidget *parent)
    : QWidget(parent)
    , m_recent(recent)
    , m_list(new QListWidget(this))
    , m_filter(new QLineEdit(this))
    , m_empty(new QLabel(this))
    , m_clear(new QPushButton(tr("Clear"), this))
{
    setObjectName(QStringLiteral("card"));
    setAttribute(Qt::WA_StyledBackground);

    auto *title = new QLabel(tr("Recent sessions"), this);
    title->setObjectName(QStringLiteral("cardTitle"));
    m_filter->setPlaceholderText(tr("Search…"));
    m_filter->setClearButtonEnabled(true);
    m_filter->setMaximumWidth(220);
    m_clear->setFlat(true);
    m_clear->setToolTip(tr("Remove all recent sessions"));

    auto *header = new QHBoxLayout;
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_filter);
    header->addWidget(m_clear);

    m_list->setViewMode(QListView::IconMode);
    m_list->setFlow(QListView::LeftToRight);
    m_list->setWrapping(true);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setUniformItemSizes(true);
    m_list->setIconSize(kIconSize);
    m_list->setGridSize(QSize(170, 128));
    m_list->setWordWrap(true);
    m_list->setSpacing(4);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setStyleSheet(QStringLiteral("QListWidget { background: transparent; }"));

    m_empty->setObjectName(QStringLiteral("cardHint"));
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setText(tr("Computers you connect to will appear here, so you do not have to type their address again."));
    m_empty->setWordWrap(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 14, 20, 14);
    layout->addLayout(header);
    layout->addWidget(m_list, 1);
    layout->addWidget(m_empty, 1);

    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        if (m_connectEnabled)
            Q_EMIT connectRequested(idOf(item));
    });
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        if (m_connectEnabled)
            Q_EMIT connectRequested(idOf(item));
    });
    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
        if (item)
            Q_EMIT deviceSelected(idOf(item));
    });
    connect(m_list, &QWidget::customContextMenuRequested, this, &RecentConnectionsView::showContextMenu);
    connect(m_filter, &QLineEdit::textChanged, this, &RecentConnectionsView::applyFilter);
    connect(m_clear, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(this, tr("Recent sessions"), tr("Remove all recent sessions from this computer?"))
            == QMessageBox::Yes)
            m_recent->clear();
    });
    connect(m_recent, &RecentConnections::changed, this, &RecentConnectionsView::rebuild);
    rebuild();
}

void RecentConnectionsView::setConnectEnabled(bool enabled)
{
    m_connectEnabled = enabled;
}

QString RecentConnectionsView::idOf(const QListWidgetItem *item) const
{
    return item ? item->data(kIdRole).toString() : QString();
}

void RecentConnectionsView::rebuild()
{
    const QString selected = idOf(m_list->currentItem());
    m_list->clear();
    const qreal dpr = devicePixelRatioF();
    for (const RecentConnections::Entry &entry : m_recent->entries()) {
        auto *item = new QListWidgetItem(deviceIcon(entry, dpr),
                                         entry.displayName() + QLatin1Char('\n') + DeviceId::format(entry.id), m_list);
        item->setData(kIdRole, entry.id);
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        QStringList tip{entry.displayName(), DeviceId::format(entry.id)};
        if (!entry.alias.isEmpty() && !entry.name.isEmpty())
            tip << tr("Device name: %1").arg(entry.name);
        if (const QString os = platformLabel(entry.platform); !os.isEmpty())
            tip << os;
        tip << tr("Last connection: %1").arg(relativeTime(entry.lastConnected));
        if (entry.connectionCount > 0)
            tip << tr("Connections: %1").arg(entry.connectionCount);
        tip << tr("Double-click to connect");
        item->setToolTip(tip.join(QLatin1Char('\n')));
        if (entry.id == selected)
            m_list->setCurrentItem(item);
    }
    const bool empty = m_recent->entries().isEmpty();
    m_list->setVisible(!empty);
    m_empty->setVisible(empty);
    m_clear->setEnabled(!empty);
    m_filter->setEnabled(!empty);
    applyFilter();
}

void RecentConnectionsView::applyFilter()
{
    const QString needle = m_filter->text().trimmed();
    const QString digits = DeviceId::normalize(needle);
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        const QString id = idOf(item);
        const bool match = needle.isEmpty() || item->text().contains(needle, Qt::CaseInsensitive)
                           || (!digits.isEmpty() && id.contains(digits));
        item->setHidden(!match);
    }
}

void RecentConnectionsView::showContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_list->itemAt(pos);
    if (!item)
        return;
    const QString id = idOf(item);
    QMenu menu(this);
    QAction *connectAction = menu.addAction(tr("Connect"));
    connectAction->setEnabled(m_connectEnabled);
    QAction *copy = menu.addAction(tr("Copy address"));
    QAction *rename = menu.addAction(tr("Rename…"));
    menu.addSeparator();
    QAction *remove = menu.addAction(tr("Remove from list"));

    QAction *chosen = menu.exec(m_list->viewport()->mapToGlobal(pos));
    if (chosen == connectAction) {
        Q_EMIT connectRequested(id);
    } else if (chosen == copy) {
        QGuiApplication::clipboard()->setText(id);
        Q_EMIT message(tr("Address copied to clipboard."));
    } else if (chosen == rename) {
        renameDevice(id);
    } else if (chosen == remove) {
        m_recent->remove(id);
    }
}

void RecentConnectionsView::renameDevice(const QString &deviceId)
{
    const RecentConnections::Entry *entry = m_recent->find(deviceId);
    if (!entry)
        return;
    bool ok = false;
    const QString alias = QInputDialog::getText(this, tr("Rename"),
                                                tr("Name for %1 (leave empty to use the device name):")
                                                    .arg(DeviceId::format(deviceId)),
                                                QLineEdit::Normal, entry->alias, &ok);
    if (ok)
        m_recent->rename(deviceId, alias);
}

} // namespace anypc
