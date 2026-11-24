#include "ui/ViewerWindow.h"

#include <QAction>
#include <QCloseEvent>
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
