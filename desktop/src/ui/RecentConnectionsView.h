#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;

namespace anypc {

class RecentConnections;

/**
 * AnyDesk-style grid of previously contacted devices. Double click (or Enter)
 * connects; the context menu offers connect, copy, rename and remove.
 */
class RecentConnectionsView final : public QWidget
{
    Q_OBJECT

public:
    explicit RecentConnectionsView(RecentConnections *recent, QWidget *parent = nullptr);

    /** Disables connecting while another request is in progress. */
    void setConnectEnabled(bool enabled);

Q_SIGNALS:
    void connectRequested(const QString &deviceId);
    void deviceSelected(const QString &deviceId);
    void message(const QString &text);

private:
    void rebuild();
    void showContextMenu(const QPoint &pos);
    void renameDevice(const QString &deviceId);
    QString idOf(const QListWidgetItem *item) const;
    void applyFilter();

    RecentConnections *m_recent;
    QListWidget *m_list;
    QLineEdit *m_filter;
    QLabel *m_empty;
    QPushButton *m_clear;
    bool m_connectEnabled = true;
};

} // namespace anypc
