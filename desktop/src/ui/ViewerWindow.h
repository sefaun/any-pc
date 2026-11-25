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
    bool isAudioMuted() const;
    bool isAudioAvailable() const;

public Q_SLOTS:
    void setSessionState(const QString &state, const QString &detail);
    /** Host screen list: {screens:[{index,name}], current}. */
    void setRemoteScreens(const QJsonObject &info);
    /** Shows the sound button once the host's sound is received. */
    void setAudioAvailable(bool available);
    void setAudioMuted(bool muted);

Q_SIGNALS:
    void disconnectRequested();
    void sendClipboardRequested();
    void sendFilesRequested();
    void showTransfersRequested();
    void screenSelected(int index);
    void audioMuteToggled(bool muted);

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void toggleFullScreen();
    void updateAudioAction();

    QWebEngineView *m_view;
    QLabel *m_status;
    QComboBox *m_screens;
    QAction *m_screensAction = nullptr;
    QAction *m_fullScreenAction;
    QAction *m_audioAction = nullptr;
    bool m_closingByUser = false;
};

} // namespace anypc
