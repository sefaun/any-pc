#include "session/ClipboardSync.h"

#include <QClipboard>
#include <QGuiApplication>

namespace anypc {

namespace {
constexpr int kPollIntervalMs = 1000;
} // namespace

ClipboardSync::ClipboardSync(QObject *parent)
    : QObject(parent)
{
    // dataChanged is not emitted for external changes on every platform (notably
    // macOS), so a light poll complements the signal.
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &ClipboardSync::checkClipboard);
    m_pollTimer.setInterval(kPollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &ClipboardSync::checkClipboard);
}

void ClipboardSync::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (enabled) {
        m_lastText = currentText();
        m_pollTimer.start();
    } else {
        m_pollTimer.stop();
    }
}

QString ClipboardSync::currentText() const
{
    return QGuiApplication::clipboard()->text().left(kMaxTextLength);
}

void ClipboardSync::applyRemote(const QString &text)
{
    if (!m_enabled || text.isEmpty() || text.size() > kMaxTextLength)
        return;
    m_lastText = text;
    if (QGuiApplication::clipboard()->text() != text)
        QGuiApplication::clipboard()->setText(text);
}

void ClipboardSync::checkClipboard()
{
    if (!m_enabled)
        return;
    const QString text = currentText();
    if (text.isEmpty() || text == m_lastText)
        return;
    m_lastText = text;
    Q_EMIT localTextChanged(text);
}

} // namespace anypc
