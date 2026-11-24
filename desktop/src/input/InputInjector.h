#pragma once

#include "input/InputEvent.h"
#include "input/KeyCodeMap.h"

#include <QList>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QSize>
#include <QString>

#include <memory>

namespace anypc {

/**
 * Injects remote keyboard and mouse input into the local desktop.
 *
 * The base class owns platform independent concerns (validation, tracking of
 * pressed keys/buttons so they can be released when a session ends, mapping
 * normalised coordinates to the captured area). Subclasses implement the
 * native calls: SendInput (Windows), Quartz Event Services (macOS) and
 * XTest (Linux/X11).
 */
class InputInjector
{
public:
    virtual ~InputInjector() = default;

    /** Creates the injector for the current platform. Never returns null. */
    static std::unique_ptr<InputInjector> create();

    /** Whether input can be injected (e.g. Wayland sessions are unsupported). */
    virtual bool isSupported() const = 0;
    /** Explains why injection is unavailable, for display to the user. */
    virtual QString unsupportedReason() const { return {}; }

    /** Size of the captured video, used to determine which area it shows. */
    void setCaptureSize(const QSize &size);

    void handle(const InputEvent &event);

    /** Releases every key and button still held down by the remote side. */
    void releaseAll();

protected:
    InputInjector() = default;

    /**
     * Native desktop areas the captured video may correspond to (primary
     * display first, then the full virtual desktop), in the coordinate space
     * the platform API expects.
     */
    virtual QList<QRectF> candidateAreas() const = 0;

    virtual void movePointer(const QPointF &nativePos) = 0;
    virtual void pointerButton(MouseButton button, bool pressed, const QPointF &nativePos) = 0;
    virtual void wheel(double dxPixels, double dyPixels) = 0;
    virtual void key(const PlatformKey &key, bool pressed) = 0;

    /** Maps normalised [0,1] coordinates into the selected native area. */
    QPointF toNative(const QPointF &normalized) const;

private:
    QRectF targetArea() const;

    QSize m_captureSize;
    QSet<QString> m_pressedKeys;
    QSet<int> m_pressedButtons;
    QPointF m_lastPosition{0.5, 0.5};
};

} // namespace anypc
