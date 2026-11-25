import 'dart:ui';

import 'package:anypc_mobile/src/input/keyboard.dart';
import 'package:anypc_mobile/src/input/remote_input.dart';
import 'package:anypc_mobile/src/input/touch_controller.dart';
import 'package:anypc_mobile/src/input/view_transform.dart';
import 'package:fake_async/fake_async.dart';
import 'package:flutter_test/flutter_test.dart';

class RecordingSink implements RemoteInputSink {
  final events = <String>[];
  @override
  void pointerMove(double x, double y) => events.add('move ${x.toStringAsFixed(2)},${y.toStringAsFixed(2)}');
  @override
  void pointerButton(int button, bool down, double x, double y) => events.add('${down ? 'down' : 'up'} $button');
  @override
  void wheel(double dx, double dy) => events.add('wheel ${dy.round()}');
  @override
  void key(String code, bool down) => events.add('key $code $down');
  @override
  void text(String text) => events.add('text $text');
}

void main() {
  const viewport = Size(400, 300);
  const frame = Size(1600, 900); // 16:9 in a 4:3 viewport -> letterboxed

  group('ViewTransform', () {
    test('fits the frame and maps points to remote coordinates', () {
      const t = ViewTransform();
      final rect = t.displayedRect(viewport, frame);
      expect(rect, const Rect.fromLTWH(0, 37.5, 400, 225));
      expect(t.toRemote(const Offset(200, 150), viewport, frame), const Offset(0.5, 0.5));
      expect(t.toRemote(const Offset(0, 0), viewport, frame), const Offset(0, 0)); // clamped from the letterbox
      expect(t.toRemote(const Offset(400, 262.5), viewport, frame), const Offset(1, 1));
    });

    test('pinch keeps the focal point under the fingers', () {
      const focal = Offset(100, 150);
      final before = const ViewTransform().toRemote(focal, viewport, frame);
      final zoomed = const ViewTransform().pinch(focal: focal, factor: 2, pan: Offset.zero, viewport: viewport, frame: frame);
      expect(zoomed.scale, 2);
      final after = zoomed.toRemote(focal, viewport, frame);
      expect((after - before).distance, lessThan(0.001));
    });

    test('zoom is clamped and resets below the minimum', () {
      final t = const ViewTransform().pinch(focal: Offset.zero, factor: 100, pan: Offset.zero, viewport: viewport, frame: frame);
      expect(t.scale, ViewTransform.maxScale);
      final back = t.pinch(focal: Offset.zero, factor: 0.01, pan: Offset.zero, viewport: viewport, frame: frame);
      expect(back.scale, 1);
      expect(back.offset, Offset.zero);
    });
  });

  group('TouchController', () {
    late RecordingSink sink;
    late TouchController touch;

    setUp(() {
      sink = RecordingSink();
      touch = TouchController(sink: sink, onTransformChanged: (_) {})
        ..viewport = viewport
        ..frame = frame;
    });

    test('tap clicks with the left button at the touched point', () {
      final now = DateTime(2026);
      touch.pointerDown(1, const Offset(200, 150), time: now);
      touch.pointerUp(1, time: now.add(const Duration(milliseconds: 80)));
      expect(sink.events, ['move 0.50,0.50', 'down 0', 'up 0']);
    });

    test('long press right clicks', () {
      fakeAsync((async) {
        touch.pointerDown(1, const Offset(100, 150));
        async.elapse(const Duration(milliseconds: 600));
        touch.pointerUp(1);
        expect(sink.events, ['move 0.25,0.50', 'down 2', 'up 2']);
      });
    });

    test('drag holds the left button', () {
      touch.pointerDown(1, const Offset(100, 150));
      touch.pointerMove(1, const Offset(200, 150));
      touch.pointerMove(1, const Offset(300, 150));
      touch.pointerUp(1);
      expect(sink.events, ['move 0.25,0.50', 'down 0', 'move 0.50,0.50', 'move 0.75,0.50', 'up 0']);
    });

    test('scroll mode turns drags into wheel events', () {
      touch.scrollMode = true;
      touch.pointerDown(1, const Offset(100, 200));
      touch.pointerMove(1, const Offset(100, 150));
      touch.pointerUp(1);
      expect(sink.events, ['wheel 100']);
    });

    test('two-finger tap right clicks, pinch zooms without input', () {
      touch.pointerDown(1, const Offset(100, 150));
      touch.pointerDown(2, const Offset(140, 150));
      touch.pointerUp(2);
      touch.pointerUp(1);
      expect(sink.events, ['move 0.25,0.50', 'down 2', 'up 2']);

      sink.events.clear();
      touch.pointerDown(1, const Offset(150, 150));
      touch.pointerDown(2, const Offset(250, 150));
      touch.pointerMove(2, const Offset(350, 150));
      touch.pointerUp(2);
      touch.pointerUp(1);
      expect(sink.events, isEmpty);
      expect(touch.transform.scale, greaterThan(1.5));
    });
  });

  group('SoftKeyboard', () {
    test('detects typed text and backspaces', () {
      expect(SoftKeyboard.diff(SoftKeyboard.sentinel).isEmpty, isTrue);
      expect(SoftKeyboard.diff('${SoftKeyboard.sentinel}ğü').inserted, 'ğü');
      expect(SoftKeyboard.diff(SoftKeyboard.sentinel.substring(1)).backspaces, 1);
      expect(SoftKeyboard.diff('').backspaces, SoftKeyboard.sentinel.length);
    });

    test('maps characters to physical keys for shortcuts', () {
      expect(KeyCodes.forCharacter('c'), 'KeyC');
      expect(KeyCodes.forCharacter('7'), 'Digit7');
      expect(KeyCodes.forCharacter('ş'), isNull);
    });
  });
}
