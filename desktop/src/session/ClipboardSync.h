#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

namespace anypc {

/**
 * Bidirectional plain-text clipboard synchronisation for one session.
 * Suppresses echo (text we just applied from the peer is not sent back).
 */
class ClipboardSync final : public QObject
{
    Q_OBJECT

public:
    static constexpr int kMaxTextLength = 16 * 1024;

    explicit ClipboardSync(QObject *parent = nullptr);

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    /** Applies text received from the peer to the local clipboard. */
    void applyRemote(const QString &text);

    /** Current local clipboard text (bounded), used for explicit "send clipboard". */
    QString currentText() const;

Q_SIGNALS:
    void localTextChanged(const QString &text);

private:
    void checkClipboard();

    QTimer m_pollTimer;
    QString m_lastText;
    bool m_enabled = false;
};

} // namespace anypc
