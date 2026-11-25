#include "ui/QrCodeWidget.h"

#include "third_party/qrcodegen/qrcodegen.hpp"

#include <QMouseEvent>
#include <QPainter>

namespace anypc {

namespace {
constexpr int kQuietZone = 4;
}

QrCodeWidget::QrCodeWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(96, 96);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("Scan with the AnyPC mobile app (click to enlarge)"));
}

void QrCodeWidget::setText(const QString &text)
{
    if (text == m_text)
        return;
    m_text = text;
    m_image = render(text);
    update();
}

QImage QrCodeWidget::render(const QString &text)
{
    if (text.isEmpty())
        return {};
    const QByteArray utf8 = text.toUtf8();
    const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(utf8.constData(), qrcodegen::QrCode::Ecc::MEDIUM);
    const int size = qr.getSize() + 2 * kQuietZone;
    QImage image(size, size, QImage::Format_Mono);
    image.setColorTable({qRgb(255, 255, 255), qRgb(0, 0, 0)});
    image.fill(0);
    for (int y = 0; y < qr.getSize(); ++y) {
        for (int x = 0; x < qr.getSize(); ++x) {
            if (qr.getModule(x, y))
                image.setPixel(x + kQuietZone, y + kQuietZone, 1);
        }
    }
    return image;
}

QSize QrCodeWidget::sizeHint() const
{
    return {112, 112};
}

void QrCodeWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()))
        Q_EMIT clicked();
    QWidget::mouseReleaseEvent(event);
}

void QrCodeWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const int side = qMin(width(), height());
    const QRect target((width() - side) / 2, (height() - side) / 2, side, side);
    painter.fillRect(target, Qt::white);
    if (m_image.isNull())
        return;
    // Scale by whole modules so every module has the same size and stays sharp.
    const int scale = qMax(1, side / m_image.width());
    const int drawn = scale * m_image.width();
    const QRect qrRect(target.x() + (side - drawn) / 2, target.y() + (side - drawn) / 2, drawn, drawn);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(qrRect, m_image);
}

} // namespace anypc
