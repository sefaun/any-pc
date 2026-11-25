#pragma once

#include <QImage>
#include <QWidget>

namespace anypc {

/** Renders text as a QR code (black modules on a white quiet zone). */
class QrCodeWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit QrCodeWidget(QWidget *parent = nullptr);

    /** Sets the encoded text; an empty string shows a blank placeholder. */
    void setText(const QString &text);
    QString text() const { return m_text; }

    /** One pixel per module, including a 4 module quiet zone; null for empty text. */
    static QImage render(const QString &text);

    QSize sizeHint() const override;
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return width; }

Q_SIGNALS:
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QString m_text;
    QImage m_image;
};

} // namespace anypc
