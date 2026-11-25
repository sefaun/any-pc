#include "input/InputInjector.h"

#include <QLoggingCategory>
#include <QScreen>

#include <cmath>
#include <limits>
#include <utility>

Q_LOGGING_CATEGORY(lcInput, "anypc.input")

namespace anypc {

void InputInjector::setCaptureSize(const QSize &size)
{
    m_captureSize = size;
    const QRectF area = targetArea();
    qCInfo(lcInput) << "Capture size" << size << "mapped to native area" << area;
}

void InputInjector::setTargetScreen(const QScreen *screen)
{
    m_targetRect = screen ? nativeRect(*screen) : QRectF();
    qCInfo(lcInput) << "Target screen" << (screen ? screen->name() : QStringLiteral("<inferred>")) << "native area"
                    << targetArea();
}

QRectF InputInjector::targetArea() const
{
    if (m_targetRect.isValid())
        return m_targetRect;
    const QList<QRectF> areas = candidateAreas();
    if (areas.isEmpty())
        return {};
    if (!m_captureSize.isValid() || m_captureSize.height() == 0)
        return areas.first();

    // Pick the area whose aspect ratio best matches the captured video.
    const double captureRatio = double(m_captureSize.width()) / m_captureSize.height();
    QRectF best = areas.first();
    double bestScore = std::numeric_limits<double>::max();
    for (const QRectF &area : areas) {
        if (area.height() <= 0)
            continue;
        const double score = std::abs(area.width() / area.height() - captureRatio);
        if (score < bestScore - 1e-3) {
            bestScore = score;
            best = area;
        }
    }
    return best;
}

QPointF InputInjector::toNative(const QPointF &normalized) const
{
    const QRectF area = targetArea();
    return {area.x() + normalized.x() * (area.width() - 1), area.y() + normalized.y() * (area.height() - 1)};
}

void InputInjector::handle(const InputEvent &event)
{
    if (!isSupported())
        return;

    switch (event.type) {
    case InputEvent::Type::PointerMove:
        m_lastPosition = event.position;
        movePointer(toNative(event.position));
        break;
    case InputEvent::Type::PointerButton: {
        const int id = static_cast<int>(event.button);
        // Ignore unbalanced events (e.g. a release whose press we never saw).
        if (event.pressed == m_pressedButtons.contains(id))
            return;
        if (event.pressed)
            m_pressedButtons.insert(id);
        else
            m_pressedButtons.remove(id);
        m_lastPosition = event.position;
        pointerButton(event.button, event.pressed, toNative(event.position));
        break;
    }
    case InputEvent::Type::Wheel:
        wheel(event.wheelDx, event.wheelDy);
        break;
    case InputEvent::Type::Key: {
        const auto key = KeyCodeMap::lookup(event.keyCode);
        if (!key) {
            qCDebug(lcInput) << "Unmapped key code" << event.keyCode;
            return;
        }
        if (event.pressed)
            m_pressedKeys.insert(event.keyCode);
        else if (!m_pressedKeys.remove(event.keyCode))
            return;
        this->key(*key, event.pressed);
        break;
    }
    case InputEvent::Type::Text:
        typeText(event.text);
        break;
    }
}

void InputInjector::releaseAll()
{
    if (!isSupported())
        return;
    const QPointF position = toNative(m_lastPosition);
    for (int id : std::as_const(m_pressedButtons))
        pointerButton(static_cast<MouseButton>(id), false, position);
    m_pressedButtons.clear();
    for (const QString &code : std::as_const(m_pressedKeys)) {
        if (const auto key = KeyCodeMap::lookup(code))
            this->key(*key, false);
    }
    m_pressedKeys.clear();
}

} // namespace anypc
