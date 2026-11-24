#pragma once

#include "network/Protocol.h"

#include <QDialog>
#include <QTimer>

class QLabel;

namespace anypc {

/** Asks the local user to accept or decline a remote control request. */
class IncomingRequestDialog final : public QDialog
{
    Q_OBJECT

public:
    IncomingRequestDialog(const QString &connectionId, const protocol::DeviceSummary &from, int expiresInMs,
                          QWidget *parent = nullptr);

    QString connectionId() const { return m_connectionId; }

Q_SIGNALS:
    void answered(const QString &connectionId, bool accepted);

protected:
    void reject() override;

private:
    void finish(bool accepted);
    void updateCountdown();

    QString m_connectionId;
    QLabel *m_countdown;
    QTimer m_timer;
    int m_remainingSeconds;
    bool m_answered = false;
};

} // namespace anypc
