#include "ui/IncomingRequestDialog.h"

#include "core/DeviceId.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace anypc {

IncomingRequestDialog::IncomingRequestDialog(const QString &connectionId, const protocol::DeviceSummary &from,
                                             int expiresInMs, QWidget *parent)
    : QDialog(parent)
    , m_connectionId(connectionId)
    , m_countdown(new QLabel(this))
    , m_remainingSeconds(qMax(1, expiresInMs / 1000))
{
    setWindowTitle(tr("Incoming connection"));
    setWindowFlag(Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(false);

    auto *icon = new QLabel(this);
    icon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxQuestion).pixmap(40, 40));

    auto *title = new QLabel(tr("<b>%1</b> wants to control this computer.").arg(from.name.toHtmlEscaped()), this);
    title->setWordWrap(true);
    auto *details = new QLabel(tr("Address: %1 · Platform: %2").arg(DeviceId::format(from.id), from.platform), this);
    details->setObjectName(QStringLiteral("cardHint"));

    auto *buttons = new QDialogButtonBox(this);
    QPushButton *accept = buttons->addButton(tr("Accept"), QDialogButtonBox::AcceptRole);
    accept->setObjectName(QStringLiteral("primary"));
    QPushButton *decline = buttons->addButton(tr("Decline"), QDialogButtonBox::RejectRole);
    connect(accept, &QPushButton::clicked, this, [this]() { finish(true); });
    connect(decline, &QPushButton::clicked, this, [this]() { finish(false); });

    auto *header = new QHBoxLayout;
    header->addWidget(icon, 0, Qt::AlignTop);
    auto *texts = new QVBoxLayout;
    texts->addWidget(title);
    texts->addWidget(details);
    texts->addWidget(m_countdown);
    header->addLayout(texts, 1);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(header);
    layout->addWidget(buttons);
    setMinimumWidth(420);

    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &IncomingRequestDialog::updateCountdown);
    m_timer.start();
    updateCountdown();
    QApplication::alert(this);
}

void IncomingRequestDialog::updateCountdown()
{
    if (m_remainingSeconds <= 0) {
        finish(false);
        return;
    }
    m_countdown->setText(tr("Declining automatically in %n second(s).", nullptr, m_remainingSeconds));
    --m_remainingSeconds;
}

void IncomingRequestDialog::reject()
{
    finish(false);
}

void IncomingRequestDialog::finish(bool accepted)
{
    if (m_answered)
        return;
    m_answered = true;
    m_timer.stop();
    Q_EMIT answered(m_connectionId, accepted);
    close();
}

} // namespace anypc
