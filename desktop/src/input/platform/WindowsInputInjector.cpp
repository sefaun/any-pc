// Windows implementation based on SendInput.
#include "input/InputInjector.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cmath>

namespace anypc {

namespace {

class WindowsInputInjector final : public InputInjector
{
public:
    bool isSupported() const override { return true; }

protected:
    QList<QRectF> candidateAreas() const override
    {
        QList<QRectF> areas;
        // Qt 6 runs per-monitor DPI aware, so these are physical pixels.
        const POINT origin{0, 0};
        HMONITOR primary = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        if (primary && GetMonitorInfoW(primary, &info)) {
            const RECT &r = info.rcMonitor;
            areas.append(QRectF(r.left, r.top, r.right - r.left, r.bottom - r.top));
        }
        areas.append(virtualDesktop());
        return areas;
    }

    void movePointer(const QPointF &pos) override
    {
        INPUT input = absoluteMouse(pos);
        input.mi.dwFlags |= MOUSEEVENTF_MOVE;
        SendInput(1, &input, sizeof(INPUT));
    }

    void pointerButton(MouseButton button, bool pressed, const QPointF &pos) override
    {
        INPUT input = absoluteMouse(pos);
        input.mi.dwFlags |= MOUSEEVENTF_MOVE;
        switch (button) {
        case MouseButton::Left:
            input.mi.dwFlags |= pressed ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
            break;
        case MouseButton::Right:
            input.mi.dwFlags |= pressed ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
            break;
        case MouseButton::Middle:
            input.mi.dwFlags |= pressed ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
            break;
        case MouseButton::Back:
        case MouseButton::Forward:
            input.mi.dwFlags |= pressed ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
            input.mi.mouseData = button == MouseButton::Back ? XBUTTON1 : XBUTTON2;
            break;
        }
        SendInput(1, &input, sizeof(INPUT));
    }

    void wheel(double dxPixels, double dyPixels) override
    {
        // Browsers report ~100 CSS px per notch; Windows expects WHEEL_DELTA (120).
        m_wheelY += -dyPixels * WHEEL_DELTA / 100.0;
        m_wheelX += dxPixels * WHEEL_DELTA / 100.0;
        sendWheel(MOUSEEVENTF_WHEEL, m_wheelY);
        sendWheel(MOUSEEVENTF_HWHEEL, m_wheelX);
    }

    void key(const PlatformKey &key, bool pressed) override
    {
        if (key.windowsVirtualKey == 0)
            return;
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = key.windowsVirtualKey;
        input.ki.wScan = static_cast<WORD>(MapVirtualKeyW(key.windowsVirtualKey, MAPVK_VK_TO_VSC));
        input.ki.dwFlags = (key.windowsExtended ? KEYEVENTF_EXTENDEDKEY : 0) | (pressed ? 0 : KEYEVENTF_KEYUP);
        SendInput(1, &input, sizeof(INPUT));
    }

private:
    static QRectF virtualDesktop()
    {
        return QRectF(GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN),
                      GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN));
    }

    static INPUT absoluteMouse(const QPointF &pos)
    {
        const QRectF desk = virtualDesktop();
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dx = static_cast<LONG>(std::lround((pos.x() - desk.x()) * 65535.0 / qMax(1.0, desk.width() - 1)));
        input.mi.dy = static_cast<LONG>(std::lround((pos.y() - desk.y()) * 65535.0 / qMax(1.0, desk.height() - 1)));
        input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
        return input;
    }

    static void sendWheel(DWORD flag, double &accumulator)
    {
        const LONG amount = static_cast<LONG>(accumulator);
        if (amount == 0)
            return;
        accumulator -= amount;
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = flag;
        input.mi.mouseData = static_cast<DWORD>(amount);
        SendInput(1, &input, sizeof(INPUT));
    }

    double m_wheelX = 0;
    double m_wheelY = 0;
};

} // namespace

std::unique_ptr<InputInjector> InputInjector::create()
{
    return std::make_unique<WindowsInputInjector>();
}

} // namespace anypc
