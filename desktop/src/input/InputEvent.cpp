#include "input/InputEvent.h"

#include <QtGlobal>

#include <cmath>

namespace anypc {

namespace {
double clampUnit(double v)
{
    return std::isfinite(v) ? qBound(0.0, v, 1.0) : 0.0;
}

double clampDelta(double v)
{
    return std::isfinite(v) ? qBound(-10000.0, v, 10000.0) : 0.0;
}

QPointF readPosition(const QJsonObject &json)
{
    return {clampUnit(json.value(QLatin1String("x")).toDouble()), clampUnit(json.value(QLatin1String("y")).toDouble())};
}
} // namespace

std::optional<InputEvent> InputEvent::fromJson(const QJsonObject &json)
{
    const QString type = json.value(QLatin1String("t")).toString();
    InputEvent event;

    if (type == QLatin1String("pm")) {
        event.type = Type::PointerMove;
        event.position = readPosition(json);
        return event;
    }
    if (type == QLatin1String("pb")) {
        const int button = json.value(QLatin1String("b")).toInt(-1);
        if (button < 0 || button > 4)
            return std::nullopt;
        event.type = Type::PointerButton;
        event.button = static_cast<MouseButton>(button);
        event.pressed = json.value(QLatin1String("d")).toBool();
        event.position = readPosition(json);
        return event;
    }
    if (type == QLatin1String("wh")) {
        event.type = Type::Wheel;
        event.wheelDx = clampDelta(json.value(QLatin1String("dx")).toDouble());
        event.wheelDy = clampDelta(json.value(QLatin1String("dy")).toDouble());
        return event;
    }
    if (type == QLatin1String("kb")) {
        event.type = Type::Key;
        event.keyCode = json.value(QLatin1String("c")).toString().left(32);
        event.pressed = json.value(QLatin1String("d")).toBool();
        if (event.keyCode.isEmpty())
            return std::nullopt;
        return event;
    }
    return std::nullopt;
}

} // namespace anypc
