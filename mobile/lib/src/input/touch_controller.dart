import 'dart:async';
import 'dart:ui';

import 'remote_input.dart';
import 'view_transform.dart';

/// Turns raw touch pointers into remote mouse input and local zoom/pan.
///
/// One finger: tap = left click, long press = right click, drag = left-button
/// drag (or scrolling while [scrollMode] is on). Two fingers: pinch to zoom
/// and pan the view; a two-finger tap is a right click.
class TouchController {
  TouchController({required this.sink, required this.onTransformChanged, Duration? longPressDelay})
      : longPressDelay = longPressDelay ?? const Duration(milliseconds: 550);

  static const double touchSlop = 10;
  static const Duration tapTimeout = Duration(milliseconds: 250);

  final RemoteInputSink sink;
  final void Function(ViewTransform transform) onTransformChanged;
  final Duration longPressDelay;

  ViewTransform transform = const ViewTransform();
  Size viewport = Size.zero;
  Size frame = Size.zero;
  bool scrollMode = false;

  final Map<int, Offset> _pointers = {};
  _Gesture _gesture = _Gesture.idle;
  Offset _downPos = Offset.zero;
  Offset _lastPos = Offset.zero;
  DateTime _downTime = DateTime.fromMillisecondsSinceEpoch(0);
  Timer? _longPress;
  bool _twoFingerMoved = false;
  double _pinchStartDistance = 0;
  Offset _pinchLastFocal = Offset.zero;
  double _pinchLastDistance = 0;

  Offset _remote(Offset p) => transform.toRemote(p, viewport, frame);

  void pointerDown(int pointer, Offset position, {DateTime? time}) {
    _pointers[pointer] = position;
    if (_pointers.length == 1) {
      _gesture = _Gesture.pending;
      _downPos = position;
      _lastPos = position;
      _downTime = time ?? DateTime.now();
      _longPress?.cancel();
      _longPress = Timer(longPressDelay, _onLongPress);
    } else if (_pointers.length == 2) {
      _longPress?.cancel();
      if (_gesture == _Gesture.dragging) _endDrag();
      _gesture = _Gesture.pinching;
      _twoFingerMoved = false;
      _pinchStartDistance = _distance();
      _pinchLastDistance = _pinchStartDistance;
      _pinchLastFocal = _focal();
    }
  }

  void pointerMove(int pointer, Offset position) {
    if (!_pointers.containsKey(pointer)) return;
    _pointers[pointer] = position;

    switch (_gesture) {
      case _Gesture.pending:
        if ((position - _downPos).distance > touchSlop) {
          _longPress?.cancel();
          _gesture = _Gesture.dragging;
          if (!scrollMode) {
            final start = _remote(_downPos);
            sink.pointerMove(start.dx, start.dy);
            sink.pointerButton(MouseButtons.left, true, start.dx, start.dy);
          }
          _dragTo(position);
        }
      case _Gesture.dragging:
        _dragTo(position);
      case _Gesture.pinching:
        if (_pointers.length < 2) return;
        final focal = _focal();
        final distance = _distance();
        if ((distance - _pinchStartDistance).abs() > touchSlop || (focal - _pinchLastFocal).distance > touchSlop) {
          _twoFingerMoved = true;
        }
        final factor = _pinchLastDistance > 0 ? distance / _pinchLastDistance : 1.0;
        transform = transform.pinch(focal: focal, factor: factor, pan: focal - _pinchLastFocal, viewport: viewport, frame: frame);
        _pinchLastFocal = focal;
        _pinchLastDistance = distance;
        onTransformChanged(transform);
      case _Gesture.idle:
      case _Gesture.consumed:
        break;
    }
  }

  void pointerUp(int pointer, {DateTime? time}) {
    if (!_pointers.containsKey(pointer)) return;
    final position = _pointers.remove(pointer)!;
    switch (_gesture) {
      case _Gesture.pending:
        _longPress?.cancel();
        final elapsed = (time ?? DateTime.now()).difference(_downTime);
        if (elapsed <= longPressDelay) _click(MouseButtons.left, position);
        _gesture = _Gesture.idle;
      case _Gesture.dragging:
        _endDrag();
        _gesture = _Gesture.idle;
      case _Gesture.pinching:
        if (_pointers.length == 1) {
          // A quick two-finger tap is a right click (common touchpad gesture).
          if (!_twoFingerMoved) _click(MouseButtons.right, _downPos);
          _gesture = _Gesture.consumed;
        }
      case _Gesture.consumed:
      case _Gesture.idle:
        break;
    }
    if (_pointers.isEmpty) _gesture = _Gesture.idle;
  }

  void pointerCancel(int pointer) {
    _pointers.remove(pointer);
    _longPress?.cancel();
    if (_gesture == _Gesture.dragging) _endDrag();
    if (_pointers.isEmpty) _gesture = _Gesture.idle;
  }

  void resetZoom() {
    transform = const ViewTransform();
    onTransformChanged(transform);
  }

  void dispose() => _longPress?.cancel();

  void _onLongPress() {
    if (_gesture != _Gesture.pending) return;
    _click(MouseButtons.right, _downPos);
    _gesture = _Gesture.consumed;
  }

  void _dragTo(Offset position) {
    if (scrollMode) {
      final delta = position - _lastPos;
      // Natural scrolling: content follows the finger.
      sink.wheel(-delta.dx * 2, -delta.dy * 2);
    } else {
      final p = _remote(position);
      sink.pointerMove(p.dx, p.dy);
    }
    _lastPos = position;
  }

  void _endDrag() {
    if (scrollMode) return;
    final p = _remote(_lastPos);
    sink.pointerButton(MouseButtons.left, false, p.dx, p.dy);
  }

  void _click(int button, Offset position) {
    final p = _remote(position);
    sink.pointerMove(p.dx, p.dy);
    sink.pointerButton(button, true, p.dx, p.dy);
    sink.pointerButton(button, false, p.dx, p.dy);
  }

  Offset _focal() {
    final points = _pointers.values.take(2).toList();
    return (points[0] + points[1]) / 2;
  }

  double _distance() {
    final points = _pointers.values.take(2).toList();
    return (points[0] - points[1]).distance;
  }
}

enum _Gesture { idle, pending, dragging, pinching, consumed }
