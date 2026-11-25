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

    // Indicator only: the person at the remote computer controls its sound.
    // Hidden until the sound arrives (the remote computer may not share any).
    m_audioAction = toolbar->addAction(QString());
    m_audioAction->setEnabled(false);
    m_audioAction->setVisible(false);
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

void ViewerWindow::setRemoteAudio(bool available, bool enabled)
{
    m_remoteAudioEnabled = enabled;
    m_audioAction->setVisible(available);
    updateAudioAction();
}

bool ViewerWindow::isAudioAvailable() const
{
    return m_audioAction->isVisible();
}

void ViewerWindow::updateAudioAction()
{
    const bool on = m_remoteAudioEnabled;
    m_audioAction->setIcon(style()->standardIcon(on ? QStyle::SP_MediaVolume : QStyle::SP_MediaVolumeMuted));
    m_audioAction->setText(on ? tr("Sound on") : tr("Sound off"));
    m_audioAction->setToolTip(on ? tr("Playing the remote computer's sound.")
                                 : tr("The person at the remote computer turned its sound off."));
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
