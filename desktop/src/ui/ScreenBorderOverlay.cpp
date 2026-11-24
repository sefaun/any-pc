#include "ui/ScreenBorderOverlay.h"

#include <QFontMetrics>
#include <QPainter>
#include <QRegion>
#include <QScreen>

namespace anypc {

namespace {
constexpr int kBorderWidth = 5;
constexpr int kCaptionPaddingX = 14;
constexpr int kCaptionHeight = 26;
const QColor kBorderColor(229, 72, 77); // #e5484d, the AnyPC accent.
} // namespace

ScreenBorderOverlay::ScreenBorderOverlay(QWidget *parent)
    : QWidget(parent,
              Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowTransparentForInput
                  | Qt::WindowDoesNotAcceptFocus | Qt::NoDropShadowWindowHint)
{
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
    setWindowTitle(QStringLiteral("AnyPC screen indicator"));
}

void ScreenBorderOverlay::showOn(QScreen *screen, const QString &caption)
{
    m_screen = screen;
    m_caption = caption;
    if (!screen) {
        hide();
        return;
    }
    // The geometry must be set before showing so the window maps on the right screen.
    setScreen(screen);
    setGeometry(screen->geometry());
    updateMask();
    show();
    raise();
    update();
}

QRect ScreenBorderOverlay::captionRect() const
{
    if (m_caption.isEmpty())
        return {};
    QFont f = font();
    f.setBold(true);
    const int textWidth = QFontMetrics(f).horizontalAdvance(m_caption);
    const int w = textWidth + 2 * kCaptionPaddingX;
    return QRect((width() - w) / 2, 0, w, kCaptionHeight);
}

void ScreenBorderOverlay::updateMask()
{
    const QRect outer = rect();
    const QRect inner = outer.adjusted(kBorderWidth, kBorderWidth, -kBorderWidth, -kBorderWidth);
    QRegion region = QRegion(outer).subtracted(QRegion(inner));
    region = region.united(captionRect());
    setMask(region);
}

void ScreenBorderOverlay::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateMask();
}

void ScreenBorderOverlay::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), kBorderColor);

    const QRect caption = captionRect();
    if (caption.isValid()) {
        QFont f = font();
        f.setBold(true);
        painter.setFont(f);
        painter.setPen(Qt::white);
        painter.drawText(caption, Qt::AlignCenter, m_caption);
    }
}

} // namespace anypc
