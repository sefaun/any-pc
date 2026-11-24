#pragma once

#include <QJsonObject>
#include <QPointF>
#include <QString>

#include <optional>

namespace anypc {

enum class MouseButton { Left = 0, Middle = 1, Right = 2, Back = 3, Forward = 4 };

/**
 * A remote input event as sent by the viewer over the control data channel.
 *
 * Wire format (JSON):
 *   {"t":"pm","x":0.5,"y":0.5}                    pointer move (normalised 0..1)
 *   {"t":"pb","b":0,"d":true,"x":..,"y":..}       pointer button down/up
 *   {"t":"wh","dx":0,"dy":120}                    wheel, CSS pixels
 *   {"t":"kb","c":"KeyA","d":true}                key by KeyboardEvent.code
 */
struct InputEvent
{
    enum class Type { PointerMove, PointerButton, Wheel, Key };

    Type type = Type::PointerMove;
    QPointF position;            ///< Normalised [0,1] coordinates.
    MouseButton button = MouseButton::Left;
    bool pressed = false;
    double wheelDx = 0;
    double wheelDy = 0;
    QString keyCode;             ///< DOM KeyboardEvent.code, e.g. "KeyA".

    static std::optional<InputEvent> fromJson(const QJsonObject &json);
};

} // namespace anypc
