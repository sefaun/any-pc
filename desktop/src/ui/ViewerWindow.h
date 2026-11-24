#pragma once

#include <QMainWindow>

class QLabel;
class QWebEnginePage;
class QWebEngineView;
class QAction;

namespace anypc {

/** Window that renders the remote desktop and captures local input. */
class ViewerWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit ViewerWindow(const QString &title, QWidget *parent = nullptr);

    void setPage(QWebEnginePage *page);

public Q_SLOTS:
    void setSessionState(const QString &state, const QString &detail);

Q_SIGNALS:
    void disconnectRequested();
    void sendClipboardRequested();

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void toggleFullScreen();

    QWebEngineView *m_view;
    QLabel *m_status;
    QAction *m_fullScreenAction;
    bool m_closingByUser = false;
};

} // namespace anypc
