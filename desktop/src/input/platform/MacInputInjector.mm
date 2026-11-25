// macOS implementation based on Quartz Event Services.
// Requires the "Accessibility" permission (System Settings > Privacy & Security).
#include "input/InputInjector.h"

#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>

#include <QElapsedTimer>
#include <QScreen>

#include <cmath>

namespace anypc {

namespace {

constexpr qint64 kDoubleClickIntervalMs = 400;
constexpr double kDoubleClickDistance = 4.0;

class MacInputInjector final : public InputInjector
{
public:
    MacInputInjector()
        : m_source(CGEventSourceCreate(kCGEventSourceStateHIDSystemState))
    {
        // Prompts the user once to grant Accessibility access if missing.
        const void *keys[] = {kAXTrustedCheckOptionPrompt};
        const void *values[] = {kCFBooleanTrue};
        CFDictionaryRef options = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1,
                                                     &kCFTypeDictionaryKeyCallBacks,
                                                     &kCFTypeDictionaryValueCallBacks);
        AXIsProcessTrustedWithOptions(options);
        CFRelease(options);
        m_clickTimer.start();
    }

    ~MacInputInjector() override
    {
        if (m_source)
            CFRelease(m_source);
    }

    bool isSupported() const override { return AXIsProcessTrusted(); }

    QString unsupportedReason() const override
    {
        return QStringLiteral("Grant AnyPC the Accessibility permission in System Settings > Privacy & "
                              "Security to allow remote control.");
    }

protected:
    QList<QRectF> candidateAreas() const override
    {
        QList<QRectF> areas;
        const CGRect main = CGDisplayBounds(CGMainDisplayID());
        areas.append(QRectF(main.origin.x, main.origin.y, main.size.width, main.size.height));

        uint32_t count = 0;
        CGDirectDisplayID displays[16];
        if (CGGetActiveDisplayList(16, displays, &count) == kCGErrorSuccess && count > 1) {
            CGRect all = CGDisplayBounds(displays[0]);
            for (uint32_t i = 1; i < count; ++i)
                all = CGRectUnion(all, CGDisplayBounds(displays[i]));
            areas.append(QRectF(all.origin.x, all.origin.y, all.size.width, all.size.height));
        }
        return areas;
    }

    QRectF nativeRect(const QScreen &screen) const override
    {
        // Qt and Quartz share the global coordinate space (points, origin at the
        // top-left of the main display).
        return QRectF(screen.geometry());
    }

    void movePointer(const QPointF &pos) override
    {
        m_position = pos;
        CGEventType type = kCGEventMouseMoved;
        CGMouseButton button = kCGMouseButtonLeft;
        if (m_buttons & 0x1) {
            type = kCGEventLeftMouseDragged;
        } else if (m_buttons & 0x4) {
            type = kCGEventRightMouseDragged;
            button = kCGMouseButtonRight;
        } else if (m_buttons) {
            type = kCGEventOtherMouseDragged;
            button = kCGMouseButtonCenter;
        }
        post(CGEventCreateMouseEvent(m_source, type, CGPointMake(pos.x(), pos.y()), button));
    }

    void pointerButton(MouseButton button, bool pressed, const QPointF &pos) override
    {
        m_position = pos;
        CGEventType type;
        CGMouseButton cgButton;
        int mask;
        switch (button) {
        case MouseButton::Left:
            type = pressed ? kCGEventLeftMouseDown : kCGEventLeftMouseUp;
            cgButton = kCGMouseButtonLeft;
            mask = 0x1;
            break;
        case MouseButton::Right:
            type = pressed ? kCGEventRightMouseDown : kCGEventRightMouseUp;
            cgButton = kCGMouseButtonRight;
            mask = 0x4;
            break;
        default:
            type = pressed ? kCGEventOtherMouseDown : kCGEventOtherMouseUp;
            cgButton = button == MouseButton::Middle ? kCGMouseButtonCenter : static_cast<CGMouseButton>(int(button));
            mask = 0x2;
            break;
        }

        if (pressed) {
            const bool sameSpot = std::hypot(pos.x() - m_lastClickPos.x(), pos.y() - m_lastClickPos.y()) <= kDoubleClickDistance;
            const bool quick = m_clickTimer.elapsed() <= kDoubleClickIntervalMs;
            m_clickCount = (sameSpot && quick && m_lastClickButton == button) ? m_clickCount + 1 : 1;
            m_lastClickPos = pos;
            m_lastClickButton = button;
            m_clickTimer.restart();
            m_buttons |= mask;
        } else {
            m_buttons &= ~mask;
        }

        CGEventRef event = CGEventCreateMouseEvent(m_source, type, CGPointMake(pos.x(), pos.y()), cgButton);
        if (event)
            CGEventSetIntegerValueField(event, kCGMouseEventClickState, m_clickCount);
        post(event);
    }

    void wheel(double dxPixels, double dyPixels) override
    {
        // Quartz: positive values scroll up/left, the DOM uses the opposite convention.
        CGEventRef event = CGEventCreateScrollWheelEvent(m_source, kCGScrollEventUnitPixel, 2,
                                                         static_cast<int32_t>(std::lround(-dyPixels)),
                                                         static_cast<int32_t>(std::lround(-dxPixels)));
        post(event);
    }

    void key(const PlatformKey &key, bool pressed) override
    {
        if (key.macKeyCode == KeyCodeMap::kMacUnmapped)
            return;
        const CGKeyCode code = key.macKeyCode;
        const CGEventFlags modifier = modifierFlag(code);

        if (modifier != 0 && code != kVK_CapsLock) {
            m_flags = pressed ? (m_flags | modifier) : (m_flags & ~modifier);
        } else if (code == kVK_CapsLock && pressed) {
            m_flags ^= kCGEventFlagMaskAlphaShift;
        }

        CGEventRef event = CGEventCreateKeyboardEvent(m_source, code, pressed);
        if (!event)
            return;
        if (modifier != 0)
            CGEventSetType(event, kCGEventFlagsChanged);
        CGEventSetFlags(event, m_flags);
        post(event);
    }

    void typeText(const QString &text) override
    {
        for (const QChar ch : text) {
            if (ch == QLatin1Char('\n') || ch == QLatin1Char('\r')) {
                post(CGEventCreateKeyboardEvent(m_source, kVK_Return, true));
                post(CGEventCreateKeyboardEvent(m_source, kVK_Return, false));
                continue;
            }
            const UniChar unit = ch.unicode();
            for (const bool down : {true, false}) {
                CGEventRef event = CGEventCreateKeyboardEvent(m_source, 0, down);
                if (!event)
                    continue;
                CGEventSetFlags(event, 0);
                CGEventKeyboardSetUnicodeString(event, 1, &unit);
                post(event);
            }
        }
    }

private:
    static CGEventFlags modifierFlag(CGKeyCode code)
    {
        switch (code) {
        case kVK_Shift:
        case kVK_RightShift: return kCGEventFlagMaskShift;
        case kVK_Control:
        case kVK_RightControl: return kCGEventFlagMaskControl;
        case kVK_Option:
        case kVK_RightOption: return kCGEventFlagMaskAlternate;
        case kVK_Command:
        case kVK_RightCommand: return kCGEventFlagMaskCommand;
        case kVK_CapsLock: return kCGEventFlagMaskAlphaShift;
        default: return 0;
        }
    }

    static void post(CGEventRef event)
    {
        if (!event)
            return;
        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);
    }

    CGEventSourceRef m_source = nullptr;
    CGEventFlags m_flags = 0;
    int m_buttons = 0;
    QPointF m_position;
    QPointF m_lastClickPos;
    MouseButton m_lastClickButton = MouseButton::Left;
    int m_clickCount = 0;
    QElapsedTimer m_clickTimer;
};

} // namespace

std::unique_ptr<InputInjector> InputInjector::create()
{
    return std::make_unique<MacInputInjector>();
}

} // namespace anypc
