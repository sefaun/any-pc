/// Receives input destined for the remote computer. Coordinates are
/// normalised to the remote screen ([0,1] on both axes).
abstract class RemoteInputSink {
  void pointerMove(double x, double y);
  void pointerButton(int button, bool down, double x, double y);
  void wheel(double dx, double dy);
  void key(String code, bool down);
  void text(String text);
}

abstract final class MouseButtons {
  static const left = 0;
  static const middle = 1;
  static const right = 2;
}
