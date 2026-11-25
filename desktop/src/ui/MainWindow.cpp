#include "ui/MainWindow.h"

#include "BuildInfo.h"
#include "core/AppSettings.h"
#include "core/DeviceId.h"
#include "core/Logging.h"
#include "core/RecentConnections.h"
#include "ui/RecentConnectionsView.h"
#include "session/SessionManager.h"
#include "ui/IncomingRequestDialog.h"
#include "ui/SettingsDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QCompleter>
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QStatusBar>
#include <QStringListModel>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QToolButton>
#include <QVBoxLayout>

namespace anypc {

namespace {
QWidget *makeCard(QWidget *parent, QVBoxLayout *&content)
{
    auto *card = new QWidget(parent);
    card->setObjectName(QStringLiteral("card"));
    card->setAttribute(Qt::WA_StyledBackground);
    content = new QVBoxLayout(card);
    content->setContentsMargins(20, 18, 20, 18);
    content->setSpacing(8);
    return card;
}

QLabel *makeLabel(const QString &text, const char *objectName, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QString::fromLatin1(objectName));
    label->setWordWrap(true);
    return label;
}
} // namespace

MainWindow::MainWindow(AppSettings *settings, SessionManager *sessions, QWidget *parent)
    : QMainWindow(parent)
    , m_settings(settings)
    , m_sessions(sessions)
{
    setWindowTitle(QString::fromLatin1(build::kAppName));
    setWindowIcon(QIcon(QStringLiteral(":/icons/anypc.svg")));
    setMinimumSize(780, 620);

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(20, 20, 20, 12);
    root->setSpacing(14);

    m_controlBanner = buildControlBanner();
    root->addWidget(m_controlBanner);

    auto *cards = new QHBoxLayout;
    cards->setSpacing(14);
    cards->addWidget(buildLocalCard(), 1);
    cards->addWidget(buildRemoteCard(), 1);
    root->addLayout(cards);

    m_recentView = new RecentConnectionsView(m_settings->recentConnections(), this);
    root->addWidget(m_recentView, 1);
    connect(m_recentView, &RecentConnectionsView::deviceSelected, this,
            [this](const QString &id) { m_remoteId->setText(DeviceId::format(id)); });
    connect(m_recentView, &RecentConnectionsView::connectRequested, this, [this](const QString &id) {
        m_remoteId->setText(DeviceId::format(id));
        startConnection();
    });
    connect(m_recentView, &RecentConnectionsView::message, this, &MainWindow::showMessage);
    connect(m_settings->recentConnections(), &RecentConnections::changed, this, &MainWindow::refreshRecent);
    setCentralWidget(central);

    m_statusDot = new QLabel(QStringLiteral("●"), this);
    m_statusDot->setObjectName(QStringLiteral("statusDot"));
    m_statusText = new QLabel(this);
    statusBar()->addWidget(m_statusDot);
    statusBar()->addWidget(m_statusText, 1);
    statusBar()->addPermanentWidget(new QLabel(QStringLiteral("v%1").arg(QString::fromLatin1(build::kVersion)), this));

    setupMenus();
    setupTray();
    setServerState(ServerState::Connecting);

    connect(m_settings, &AppSettings::unattendedAccessChanged, this, &MainWindow::refreshUnattendedLabel);
    connect(m_sessions, &SessionManager::incomingRequest, this, &MainWindow::showIncomingRequest);
    connect(m_sessions, &SessionManager::incomingRequestClosed, this, [this](const QString &connectionId) {
        if (auto dialog = m_incomingDialogs.take(connectionId))
            dialog->close();
    });
    connect(m_sessions, &SessionManager::outgoingPending, this, [this](const protocol::DeviceSummary &target) {
        setOutgoingBusy(true, tr("Waiting for %1 to accept…").arg(target.name));
    });
    connect(m_sessions, &SessionManager::outgoingFinished, this, [this]() { setOutgoingBusy(false); });
    connect(m_sessions, &SessionManager::hostSessionStarted, this, [this](const protocol::DeviceSummary &viewer) {
        m_controlLabel->setText(tr("<b>%1</b> (%2) is controlling this computer.")
                                    .arg(viewer.name.toHtmlEscaped(), DeviceId::format(viewer.id)));
        m_controlBanner->show();
        if (m_tray)
            m_tray->showMessage(windowTitle(), tr("%1 is now controlling this computer.").arg(viewer.name));
    });
    connect(m_sessions, &SessionManager::hostSessionEnded, m_controlBanner, &QWidget::hide);
    connect(m_sessions, &SessionManager::notification, this, &MainWindow::showMessage);
    connect(m_sessions, &SessionManager::sessionEndedRemotely, this, &MainWindow::notifySessionEnded);
}

QWidget *MainWindow::buildLocalCard()
{
    QVBoxLayout *content = nullptr;
    QWidget *card = makeCard(this, content);

    content->addWidget(makeLabel(tr("This computer"), "cardTitle", card));
    content->addWidget(makeLabel(tr("Share this address to let others connect to this computer."), "cardHint", card));

    auto *idRow = new QHBoxLayout;
    m_deviceId = new QLabel(QStringLiteral("— — —"), card);
    m_deviceId->setObjectName(QStringLiteral("deviceId"));
    m_deviceId->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *copy = new QToolButton(card);
    copy->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    copy->setToolTip(tr("Copy address"));
    connect(copy, &QToolButton::clicked, this, [this]() {
        if (m_rawDeviceId.isEmpty())
            return;
        QGuiApplication::clipboard()->setText(m_rawDeviceId);
        showMessage(tr("Address copied to clipboard."));
    });
    idRow->addWidget(m_deviceId);
    idRow->addWidget(copy);
    idRow->addStretch();
    content->addLayout(idRow);

    m_unattended = makeLabel(QString(), "cardHint", card);
    content->addWidget(m_unattended);
    auto *configure = new QPushButton(tr("Configure unattended access…"), card);
    connect(configure, &QPushButton::clicked, this, &MainWindow::openSettings);
    content->addWidget(configure, 0, Qt::AlignLeft);
    content->addStretch();
    refreshUnattendedLabel();
    return card;
}

QWidget *MainWindow::buildRemoteCard()
{
    QVBoxLayout *content = nullptr;
    QWidget *card = makeCard(this, content);

    content->addWidget(makeLabel(tr("Remote computer"), "cardTitle", card));
    content->addWidget(makeLabel(tr("Enter the address of the computer you want to control."), "cardHint", card));

    m_remoteId = new QLineEdit(card);
    m_remoteId->setObjectName(QStringLiteral("remoteId"));
    m_remoteId->setPlaceholderText(QStringLiteral("123 456 789"));
    m_remoteId->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[0-9 ]{0,11}")), m_remoteId));
    m_remoteId->setClearButtonEnabled(true);
    auto *completer = new QCompleter(new QStringListModel(m_remoteId), m_remoteId);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    m_remoteId->setCompleter(completer);
    content->addWidget(m_remoteId);

    m_remotePassword = new QLineEdit(card);
    m_remotePassword->setEchoMode(QLineEdit::Password);
    m_remotePassword->setPlaceholderText(tr("Password (optional, for unattended access)"));
    content->addWidget(m_remotePassword);

    auto *buttons = new QHBoxLayout;
    m_connectButton = new QPushButton(tr("Connect"), card);
    m_connectButton->setObjectName(QStringLiteral("primary"));
    m_connectButton->setDefault(true);
    m_cancelButton = new QPushButton(tr("Cancel"), card);
    m_cancelButton->hide();
    buttons->addWidget(m_connectButton);
    buttons->addWidget(m_cancelButton);
    buttons->addStretch();
    content->addLayout(buttons);

    m_outgoingStatus = makeLabel(QString(), "cardHint", card);
    content->addWidget(m_outgoingStatus);
    content->addStretch();

    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::startConnection);
    connect(m_remoteId, &QLineEdit::returnPressed, this, &MainWindow::startConnection);
    connect(m_remotePassword, &QLineEdit::returnPressed, this, &MainWindow::startConnection);
    connect(m_cancelButton, &QPushButton::clicked, m_sessions, &SessionManager::cancelOutgoing);
    refreshRecent();
    return card;
}

QFrame *MainWindow::buildControlBanner()
{
    auto *banner = new QFrame(this);
    banner->setObjectName(QStringLiteral("controlBanner"));
    auto *layout = new QHBoxLayout(banner);
    layout->setContentsMargins(14, 8, 8, 8);
    m_controlLabel = new QLabel(banner);
    auto *sendFiles = new QPushButton(tr("Send files…"), banner);
    connect(sendFiles, &QPushButton::clicked, this, [this]() { m_sessions->sendFilesToViewer(this); });
    auto *transfers = new QPushButton(tr("Transfers"), banner);
    connect(transfers, &QPushButton::clicked, m_sessions, &SessionManager::showHostTransfers);
    auto *end = new QPushButton(tr("Disconnect"), banner);
    connect(end, &QPushButton::clicked, m_sessions, &SessionManager::endHostSession);
    layout->addWidget(m_controlLabel, 1);
    layout->addWidget(sendFiles);
    layout->addWidget(transfers);
    layout->addWidget(end);
    banner->hide();
    return banner;
}

void MainWindow::setupMenus()
{
    QMenu *file = menuBar()->addMenu(tr("&File"));
    file->addAction(tr("&Settings…"), this, &MainWindow::openSettings);
    file->addSeparator();
    file->addAction(tr("&Quit"), QKeySequence::Quit, this, &MainWindow::quit);

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("Open &log folder"), this,
                    []() { QDesktopServices::openUrl(QUrl::fromLocalFile(Logging::directory())); });
    help->addSeparator();
    help->addAction(tr("&About AnyPC"), this, &MainWindow::showAbout);
}

void MainWindow::setupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;
    m_tray = new QSystemTrayIcon(windowIcon(), this);
    m_tray->setToolTip(windowTitle());
    auto *menu = new QMenu(this);
    menu->addAction(tr("Open AnyPC"), this, &MainWindow::bringToFront);
    menu->addSeparator();
    menu->addAction(tr("Quit"), this, &MainWindow::quit);
    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            bringToFront();
    });
    m_tray->show();
}

void MainWindow::setDeviceId(const QString &deviceId)
{
    m_rawDeviceId = deviceId;
    m_deviceId->setText(deviceId.isEmpty() ? QStringLiteral("— — —") : DeviceId::format(deviceId));
    if (m_tray && !deviceId.isEmpty())
        m_tray->setToolTip(QStringLiteral("%1 — %2").arg(windowTitle(), DeviceId::format(deviceId)));
}

void MainWindow::setServerState(ServerState state, const QString &detail)
{
    QString color;
    QString text;
    switch (state) {
    case ServerState::Online:
        color = QStringLiteral("#30a46c");
        text = tr("Ready to connect (secure connection to server).");
        break;
    case ServerState::Connecting:
        color = QStringLiteral("#f5a524");
        text = tr("Connecting to server…");
        break;
    case ServerState::Offline:
        color = QStringLiteral("#e5484d");
        text = tr("Offline");
        break;
    }
    if (!detail.isEmpty())
        text += QStringLiteral(" ") + detail;
    m_statusDot->setStyleSheet(QStringLiteral("color: %1;").arg(color));
    m_statusText->setText(text);
    m_connectButton->setEnabled(state == ServerState::Online && !m_sessions->hasOutgoingRequest());
}

void MainWindow::notifySessionEnded(const protocol::DeviceSummary &peer, protocol::Role role, const QString &reason)
{
    const QString who = QStringLiteral("%1 (%2)").arg(peer.name, DeviceId::format(peer.id));
    const QString title = role == protocol::Role::Host ? tr("Remote control ended") : tr("Connection closed");
    const QString text = role == protocol::Role::Host
                             ? tr("%1 is no longer connected to this computer.").arg(who)
                             : tr("The session with %1 has ended.").arg(who);

    if (m_tray)
        m_tray->showMessage(title, text + QLatin1Char('\n') + reason, QSystemTrayIcon::Information, 8000);

    // A non-modal box stays visible until acknowledged, so the event is not missed.
    auto *box = new QMessageBox(QMessageBox::Information, title, text, QMessageBox::Ok, this);
    box->setInformativeText(reason);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setWindowModality(Qt::NonModal);
    box->setWindowFlag(Qt::WindowStaysOnTopHint);
    box->show();
    box->raise();
    QApplication::alert(box);
}

void MainWindow::showMessage(const QString &message)
{
    if (!message.isEmpty())
        statusBar()->showMessage(message, 6000);
}

void MainWindow::bringToFront()
{
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::refreshUnattendedLabel()
{
    m_unattended->setText(m_settings->hasUnattendedPassword()
                              ? tr("Unattended access is enabled (password protected).")
                              : tr("Unattended access is disabled; incoming requests must be accepted."));
}

void MainWindow::refreshRecent()
{
    QStringList formatted;
    for (const RecentConnections::Entry &entry : m_settings->recentConnections()->entries())
        formatted.append(DeviceId::format(entry.id));
    if (auto *model = qobject_cast<QStringListModel *>(m_remoteId->completer()->model()))
        model->setStringList(formatted);
}

void MainWindow::startConnection()
{
    const QString id = DeviceId::normalize(m_remoteId->text());
    if (!DeviceId::isValid(id)) {
        showMessage(tr("Please enter a valid 9 digit address."));
        m_remoteId->setFocus();
        return;
    }
    if (id == m_rawDeviceId) {
        showMessage(tr("You cannot connect to this computer itself."));
        return;
    }
    setOutgoingBusy(true, tr("Requesting connection…"));
    m_sessions->connectTo(id, m_remotePassword->text());
    m_remotePassword->clear();
    refreshRecent();
    if (!m_sessions->hasOutgoingRequest())
        setOutgoingBusy(false);
}

void MainWindow::setOutgoingBusy(bool busy, const QString &text)
{
    m_connectButton->setEnabled(!busy);
    m_cancelButton->setVisible(busy);
    if (m_recentView)
        m_recentView->setConnectEnabled(!busy);
    m_remoteId->setEnabled(!busy);
    m_remotePassword->setEnabled(!busy);
    m_outgoingStatus->setText(text);
}

void MainWindow::showIncomingRequest(const QString &connectionId, const protocol::DeviceSummary &from, int expiresInMs)
{
    auto *dialog = new IncomingRequestDialog(connectionId, from, expiresInMs, this);
    m_incomingDialogs.insert(connectionId, dialog);
    connect(dialog, &IncomingRequestDialog::answered, this, [this](const QString &id, bool accepted) {
        m_incomingDialogs.remove(id);
        m_sessions->respond(id, accepted);
    });
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    if (m_tray)
        m_tray->showMessage(windowTitle(), tr("%1 wants to connect.").arg(from.name));
}

void MainWindow::openSettings()
{
    SettingsDialog dialog(m_settings, this);
    dialog.exec();
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("About AnyPC"),
                       tr("<h3>AnyPC %1</h3><p>Cross-platform remote desktop built with Qt and "
                          "Cloudflare Realtime.</p>")
                           .arg(QString::fromLatin1(build::kVersion)));
}

void MainWindow::quit()
{
    m_quitting = true;
    m_sessions->endAll();
    QApplication::quit();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!m_quitting && m_tray && m_settings->minimizeToTray()) {
        hide();
        event->ignore();
        return;
    }
    m_sessions->endAll();
    QMainWindow::closeEvent(event);
    QApplication::quit();
}

} // namespace anypc
