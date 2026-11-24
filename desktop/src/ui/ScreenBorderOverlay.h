#pragma once

#include <QPointer>
#include <QWidget>

class QScreen;

namespace anypc {

/**
 * Click-through frame drawn around the monitor that is being shared, so the
 * local user can see which screen the remote side is viewing/controlling.
 *
 * The window is masked to the border ring (plus a small caption tab), so it
 * works without a compositor and never blocks clicks on the desktop.
 */
class ScreenBorderOverlay final : public QWidget
{
    Q_OBJECT

public:
    explicit ScreenBorderOverlay(QWidget *parent = nullptr);

    /** Shows the frame on `screen` (hidden when null). */
    void showOn(QScreen *screen, const QString &caption);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QRect captionRect() const;
    void updateMask();

    QPointer<QScreen> m_screen;
    QString m_caption;
};

} // namespace anypc
