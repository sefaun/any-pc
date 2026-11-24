// Linux implementation based on the XTest extension (X11 sessions only).
#include "input/InputInjector.h"

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QScreen>

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

#include <cmath>

// Xlib defines macros that clash with Qt/C++ identifiers.
#undef None
#undef Bool
#undef Status
#undef KeyPress
#undef KeyRelease

namespace anypc {

namespace {

constexpr unsigned long kNoKeySym = 0;
constexpr KeySym kIsoLevel3Shift = 0xfe03; // AltGr on many layouts (e.g. Turkish).

class X11InputInjector final : public InputInjector
{
public:
    X11InputInjector()
    {
        const QString platform = QGuiApplication::platformName();
        if (platform != QLatin1String("xcb")) {
            m_reason = QStringLiteral("Remote control requires an X11 session (current platform: %1).").arg(platform);
            return;
        }
        m_display = XOpenDisplay(nullptr);
        if (!m_display) {
            m_reason = QStringLiteral("Unable to open the X11 display.");
            return;
        }
        int eventBase = 0, errorBase = 0, major = 0, minor = 0;
        if (!XTestQueryExtension(m_display, &eventBase, &errorBase, &major, &minor)) {
            m_reason = QStringLiteral("The XTest extension is not available on this X server.");
            XCloseDisplay(m_display);
            m_display = nullptr;
        }
    }

    ~X11InputInjector() override
    {
        if (m_display)
            XCloseDisplay(m_display);
    }

    bool isSupported() const override { return m_display != nullptr; }
    QString unsupportedReason() const override { return m_reason; }

protected:
    QList<QRectF> candidateAreas() const override
    {
        QList<QRectF> areas;
        if (const QScreen *primary = QGuiApplication::primaryScreen()) {
            const qreal dpr = primary->devicePixelRatio();
            const QRect g = primary->geometry();
            areas.append(QRectF(g.x() * dpr, g.y() * dpr, g.width() * dpr, g.height() * dpr));
        }
        const int screen = DefaultScreen(m_display);
        areas.append(QRectF(0, 0, DisplayWidth(m_display, screen), DisplayHeight(m_display, screen)));
        return areas;
    }

    QRectF nativeRect(const QScreen &screen) const override
    {
        const qreal dpr = screen.devicePixelRatio();
        const QRect g = screen.geometry();
        return QRectF(g.x() * dpr, g.y() * dpr, g.width() * dpr, g.height() * dpr);
    }

    void movePointer(const QPointF &pos) override
    {
        XTestFakeMotionEvent(m_display, -1, int(std::lround(pos.x())), int(std::lround(pos.y())), CurrentTime);
        XFlush(m_display);
    }

    void pointerButton(MouseButton button, bool pressed, const QPointF &pos) override
    {
        movePointer(pos);
        XTestFakeButtonEvent(m_display, x11Button(button), pressed ? True : False, CurrentTime);
        XFlush(m_display);
    }

    void wheel(double dxPixels, double dyPixels) override
    {
        // One X11 wheel "click" per ~100 CSS pixels; buttons 4/5 vertical, 6/7 horizontal.
        m_wheelY += dyPixels / 100.0;
        m_wheelX += dxPixels / 100.0;
        emitClicks(m_wheelY, 5, 4);
        emitClicks(m_wheelX, 7, 6);
        XFlush(m_display);
    }

    void key(const PlatformKey &key, bool pressed) override
    {
        if (key.x11KeySym == kNoKeySym)
            return;
        KeyCode code = XKeysymToKeycode(m_display, key.x11KeySym);
        if (code == 0 && key.x11KeySym == XK_Alt_R)
            code = XKeysymToKeycode(m_display, kIsoLevel3Shift);
        if (code == 0)
            return;
        XTestFakeKeyEvent(m_display, code, pressed ? True : False, CurrentTime);
        XFlush(m_display);
    }

private:
    static unsigned int x11Button(MouseButton button)
    {
        switch (button) {
        case MouseButton::Left: return 1;
        case MouseButton::Middle: return 2;
        case MouseButton::Right: return 3;
        case MouseButton::Back: return 8;
        case MouseButton::Forward: return 9;
        }
        return 1;
    }

    void emitClicks(double &accumulator, unsigned int positiveButton, unsigned int negativeButton)
    {
        while (accumulator >= 1.0) {
            click(positiveButton);
            accumulator -= 1.0;
        }
        while (accumulator <= -1.0) {
            click(negativeButton);
            accumulator += 1.0;
        }
    }

    void click(unsigned int button)
    {
        XTestFakeButtonEvent(m_display, button, True, CurrentTime);
        XTestFakeButtonEvent(m_display, button, False, CurrentTime);
    }

    Display *m_display = nullptr;
    QString m_reason;
    double m_wheelX = 0;
    double m_wheelY = 0;
};

} // namespace

std::unique_ptr<InputInjector> InputInjector::create()
{
    return std::make_unique<X11InputInjector>();
}

} // namespace anypc
