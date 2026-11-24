#pragma once

#include <QJsonObject>
#include <QMainWindow>

class QComboBox;
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
    /** Host screen list: {screens:[{index,name}], current}. */
    void setRemoteScreens(const QJsonObject &info);

Q_SIGNALS:
    void disconnectRequested();
    void sendClipboardRequested();
    void sendFilesRequested();
    void showTransfersRequested();
    void screenSelected(int index);

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void toggleFullScreen();

    QWebEngineView *m_view;
    QLabel *m_status;
    QComboBox *m_screens;
    QAction *m_screensAction = nullptr;
    QAction *m_fullScreenAction;
    bool m_closingByUser = false;
};

} // namespace anypc
