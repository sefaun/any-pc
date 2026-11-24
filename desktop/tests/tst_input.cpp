#include "input/InputEvent.h"
#include "input/KeyCodeMap.h"

#include <QJsonObject>
#include <QtTest>

using namespace anypc;

class TestInput : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesPointerMoveAndClamps()
    {
        const auto e = InputEvent::fromJson({{"t", "pm"}, {"x", 1.7}, {"y", -0.2}});
        QVERIFY(e.has_value());
        QCOMPARE(e->type, InputEvent::Type::PointerMove);
        QCOMPARE(e->position, QPointF(1.0, 0.0));
    }

    void parsesButtonsWheelAndKeys()
    {
        const auto b = InputEvent::fromJson({{"t", "pb"}, {"b", 2}, {"d", true}, {"x", 0.5}, {"y", 0.25}});
        QVERIFY(b.has_value());
        QCOMPARE(b->button, MouseButton::Right);
        QVERIFY(b->pressed);

        const auto w = InputEvent::fromJson({{"t", "wh"}, {"dx", 0}, {"dy", 120}});
        QVERIFY(w.has_value());
        QCOMPARE(w->wheelDy, 120.0);

        const auto k = InputEvent::fromJson({{"t", "kb"}, {"c", "KeyA"}, {"d", false}});
        QVERIFY(k.has_value());
        QCOMPARE(k->keyCode, QStringLiteral("KeyA"));
        QVERIFY(!k->pressed);
    }

    void rejectsInvalidEvents()
    {
        QVERIFY(!InputEvent::fromJson({{"t", "pb"}, {"b", 9}}).has_value());
        QVERIFY(!InputEvent::fromJson({{"t", "kb"}, {"c", ""}}).has_value());
        QVERIFY(!InputEvent::fromJson({{"t", "nope"}}).has_value());
    }

    void keyMapCoversCommonKeys()
    {
        for (const char *code : {"KeyA", "KeyZ", "Digit0", "Enter", "Escape", "ShiftLeft", "ControlRight", "AltRight",
                                 "MetaLeft", "ArrowUp", "F12", "Numpad5", "Backspace", "Delete", "Space"}) {
            QVERIFY2(KeyCodeMap::lookup(QString::fromLatin1(code)).has_value(), code);
        }
        const auto a = KeyCodeMap::lookup(QStringLiteral("KeyA"));
        QCOMPARE(a->windowsVirtualKey, std::uint16_t(0x41));
        QCOMPARE(a->macKeyCode, std::uint16_t(0x00));
        QCOMPARE(a->x11KeySym, std::uint32_t(0x61));
        QVERIFY(KeyCodeMap::lookup(QStringLiteral("ArrowLeft"))->windowsExtended);
        QVERIFY(!KeyCodeMap::lookup(QStringLiteral("Unknown")).has_value());
    }
};

QTEST_GUILESS_MAIN(TestInput)
#include "tst_input.moc"
