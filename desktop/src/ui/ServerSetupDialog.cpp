#include "ui/ServerSetupDialog.h"

#include "BuildInfo.h"
#include "core/AppSettings.h"

#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QVBoxLayout>

namespace anypc {

namespace {
constexpr int kHealthTimeoutMs = 10000;
} // namespace

ServerSetupDialog::ServerSetupDialog(const QUrl &initial, QWidget *parent)
    : QDialog(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_address(new QLineEdit(this))
    , m_status(new QLabel(this))
    , m_connectButton(new QPushButton(tr("Connect"), this))
    , m_saveAnywayButton(new QPushButton(tr("Save anyway"), this))
{
    setWindowTitle(tr("AnyPC server"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/anypc.svg")));
    setMinimumWidth(460);
    m_network->setTransferTimeout(kHealthTimeoutMs);

    auto *title = new QLabel(tr("Connect to your AnyPC server"), this);
    title->setObjectName(QStringLiteral("cardTitle"));
    auto *hint = new QLabel(tr("Enter the address of the AnyPC server. Both computers that want to "
                               "connect to each other must use the same server."),
                            this);
    hint->setObjectName(QStringLiteral("cardHint"));
    hint->setWordWrap(true);

    m_address->setPlaceholderText(QStringLiteral("anypc.example.com"));
    m_address->setClearButtonEnabled(true);
    if (initial.isValid())
        m_address->setText(initial.toString());

    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_status->hide();

    m_connectButton->setObjectName(QStringLiteral("primary"));
    m_connectButton->setDefault(true);
    m_saveAnywayButton->hide();

    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(m_connectButton, QDialogButtonBox::AcceptRole);
    buttons->addButton(m_saveAnywayButton, QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Cancel);

    connect(m_connectButton, &QPushButton::clicked, this, &ServerSetupDialog::verify);
    connect(m_saveAnywayButton, &QPushButton::clicked, this, [this]() {
        m_serverUrl = m_candidate;
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_address, &QLineEdit::textEdited, this, [this]() {
        m_status->hide();
        m_saveAnywayButton->hide();
    });

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->addWidget(title);
    layout->addWidget(hint);
    layout->addWidget(m_address);
    layout->addWidget(m_status);
    layout->addSpacing(6);
    layout->addWidget(buttons);
}

void ServerSetupDialog::verify()
{
    m_candidate = AppSettings::normalizeServerUrl(m_address->text());
    if (!m_candidate.isValid()) {
        showError(tr("Please enter a valid address, for example anypc.example.com."), false);
        return;
    }
    m_address->setText(m_candidate.toString());

    QUrl health = m_candidate;
    health.setPath(health.path() + QStringLiteral("/api/v1/health"));
    QNetworkRequest request(health);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("AnyPC/%1").arg(QString::fromLatin1(build::kVersion)));

    setBusy(true);
    m_status->setText(tr("Checking %1…").arg(m_candidate.host()));
    m_status->setStyleSheet(QString());
    m_status->show();
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, &ServerSetupDialog::onReplyFinished);
}

void ServerSetupDialog::onReplyFinished()
{
    QNetworkReply *reply = m_reply;
    if (!reply)
        return;
    reply->deleteLater();
    setBusy(false);

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QJsonObject body = QJsonDocument::fromJson(reply->readAll()).object();
    if (reply->error() == QNetworkReply::NoError && status == 200
        && body.value(QLatin1String("status")).toString() == QLatin1String("ok")) {
        m_serverUrl = m_candidate;
        accept();
        return;
    }

    const QString reason = reply->error() != QNetworkReply::NoError
                               ? reply->errorString()
                               : tr("The address did not respond like an AnyPC server (HTTP %1).").arg(status);
    showError(tr("Could not reach the AnyPC server: %1").arg(reason), true);
}

void ServerSetupDialog::setBusy(bool busy)
{
    m_address->setEnabled(!busy);
    m_connectButton->setEnabled(!busy);
    m_saveAnywayButton->setEnabled(!busy);
    if (!busy) {
        // Disabling the field moved focus away; put it back so Enter retries.
        m_address->setFocus();
        m_connectButton->setDefault(true);
    }
}

void ServerSetupDialog::showError(const QString &message, bool allowSaveAnyway)
{
    m_status->setText(message);
    m_status->setStyleSheet(QStringLiteral("color: #e5484d;"));
    m_status->show();
    m_saveAnywayButton->setVisible(allowSaveAnyway);
}

} // namespace anypc
