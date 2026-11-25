#include "ui/ViewerWindow.h"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QJsonArray>
#include <QSignalBlocker>
#include <QGuiApplication>
#include <QLabel>
#include <QScreen>
#include <QStyle>
#include <QToolBar>
#include <QWebEngineView>

namespace anypc {

ViewerWindow::ViewerWindow(const QString &title, QWidget *parent)
    : QMainWindow(parent)
    , m_view(new QWebEngineView(this))
    , m_status(new QLabel(this))
    , m_screens(new QComboBox(this))
{
    setWindowTitle(title);
    setMinimumSize(640, 400);

    auto *toolbar = addToolBar(tr("Session"));
    toolbar->setObjectName(QStringLiteral("sessionToolbar"));
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    // Actions intentionally have no keyboard shortcuts: every key goes to the remote side.
    toolbar->addAction(style()->standardIcon(QStyle::SP_DialogCloseButton), tr("Disconnect"), this,
                       &ViewerWindow::disconnectRequested);
    m_fullScreenAction = toolbar->addAction(style()->standardIcon(QStyle::SP_TitleBarMaxButton),
                                            tr("Full screen"), this, &ViewerWindow::toggleFullScreen);
    toolbar->addAction(style()->standardIcon(QStyle::SP_FileDialogContentsView), tr("Send clipboard"), this,
                       &ViewerWindow::sendClipboardRequested);
    toolbar->addAction(style()->standardIcon(QStyle::SP_ArrowUp), tr("Send files"), this,
                       &ViewerWindow::sendFilesRequested);
    toolbar->addAction(style()->standardIcon(QStyle::SP_FileDialogListView), tr("Transfers"), this,
                       &ViewerWindow::showTransfersRequested);

    // Checked = muted. Hidden until the host's sound arrives (it may not share sound).
    m_audioAction = toolbar->addAction(QString());
    m_audioAction->setCheckable(true);
    m_audioAction->setVisible(false);
    connect(m_audioAction, &QAction::toggled, this, [this](bool muted) {
        updateAudioAction();
        Q_EMIT audioMuteToggled(muted);
        m_view->setFocus();
    });
    updateAudioAction();

    toolbar->addSeparator();
    m_screens->setToolTip(tr("Remote screen"));
    m_screens->setFocusPolicy(Qt::NoFocus);
    m_screens->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_screensAction = toolbar->addWidget(m_screens);
    m_screensAction->setVisible(false);
    connect(m_screens, QOverload<int>::of(&QComboBox::activated), this, [this](int row) {
        Q_EMIT screenSelected(m_screens->itemData(row).toInt());
        m_view->setFocus();
    });

    auto *spacer = new QWidget(toolbar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);
    m_status->setContentsMargins(8, 0, 8, 0);
    toolbar->addWidget(m_status);

    m_view->setContextMenuPolicy(Qt::NoContextMenu);
    m_view->setFocusPolicy(Qt::StrongFocus);
    setCentralWidget(m_view);

    if (const QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect available = screen->availableGeometry();
        resize(available.width() * 3 / 4, available.height() * 3 / 4);
        move(available.center() - rect().center());
    }
    setSessionState(QStringLiteral("connecting"), {});
}

void ViewerWindow::setPage(QWebEnginePage *page)
{
    m_view->setPage(page);
    m_view->setFocus();
}

void ViewerWindow::setSessionState(const QString &state, const QString &detail)
{
    QString text;
    if (state == QLatin1String("connected"))
        text = tr("Connected");
    else if (state == QLatin1String("failed"))
        text = tr("Connection failed");
    else if (state == QLatin1String("closed"))
        text = tr("Disconnected");
    else
        text = tr("Connecting…");
    if (!detail.isEmpty() && state != QLatin1String("connected"))
        text += QStringLiteral(" — ") + detail;
    m_status->setText(text);
}

void ViewerWindow::setRemoteScreens(const QJsonObject &info)
{
    const QJsonArray screens = info.value(QLatin1String("screens")).toArray();
    const int current = info.value(QLatin1String("current")).toInt();
    const QSignalBlocker blocker(m_screens);
    m_screens->clear();
    for (const QJsonValue &value : screens) {
        const QJsonObject screen = value.toObject();
        const int index = screen.value(QLatin1String("index")).toInt();
        const QString name = screen.value(QLatin1String("name")).toString();
        m_screens->addItem(name.isEmpty() ? tr("Screen %1").arg(index + 1) : name, index);
        if (index == current)
            m_screens->setCurrentIndex(m_screens->count() - 1);
    }
    // Only worth showing when there is something to choose from.
    m_screensAction->setVisible(m_screens->count() > 1);
}

void ViewerWindow::setAudioAvailable(bool available)
{
    m_audioAction->setVisible(available);
}

void ViewerWindow::setAudioMuted(bool muted)
{
    const QSignalBlocker blocker(m_audioAction);
    m_audioAction->setChecked(muted);
    updateAudioAction();
}

bool ViewerWindow::isAudioMuted() const
{
    return m_audioAction->isChecked();
}

bool ViewerWindow::isAudioAvailable() const
{
    return m_audioAction->isVisible();
}

void ViewerWindow::updateAudioAction()
{
    const bool muted = m_audioAction->isChecked();
    m_audioAction->setIcon(style()->standardIcon(muted ? QStyle::SP_MediaVolumeMuted : QStyle::SP_MediaVolume));
    m_audioAction->setText(muted ? tr("Sound off") : tr("Sound on"));
    m_audioAction->setToolTip(muted ? tr("The remote computer's sound is muted. Click to turn it on.")
                                    : tr("Playing the remote computer's sound. Click to mute it."));
}

void ViewerWindow::toggleFullScreen()
{
    if (isFullScreen())
        showNormal();
    else
        showFullScreen();
    m_view->setFocus();
}

void ViewerWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        m_fullScreenAction->setText(isFullScreen() ? tr("Exit full screen") : tr("Full screen"));
    } else if (event->type() == QEvent::ActivationChange && isActiveWindow()) {
        m_view->setFocus();
    }
    QMainWindow::changeEvent(event);
}

void ViewerWindow::closeEvent(QCloseEvent *event)
{
    // Closing the window ends the session; the owner destroys the window.
    event->ignore();
    hide();
    Q_EMIT disconnectRequested();
}

} // namespace anypc
