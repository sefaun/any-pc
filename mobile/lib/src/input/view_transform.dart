import 'dart:math' as math;
import 'dart:ui';

/// Zoom/pan state of the remote screen inside the viewport.
///
/// The remote frame is first fitted ("contain") into the viewport, then
/// scaled by [scale] around the viewport origin and shifted by [offset].
class ViewTransform {
  const ViewTransform({this.scale = 1, this.offset = Offset.zero});

  static const double minScale = 1;
  static const double maxScale = 5;

  final double scale;
  final Offset offset;

  /// Rectangle of the fitted remote frame at scale 1.
  static Rect fittedRect(Size viewport, Size frame) {
    if (frame.isEmpty || viewport.isEmpty) return Offset.zero & viewport;
    final s = math.min(viewport.width / frame.width, viewport.height / frame.height);
    final w = frame.width * s;
    final h = frame.height * s;
    return Rect.fromLTWH((viewport.width - w) / 2, (viewport.height - h) / 2, w, h);
  }

  /// Where the remote frame is drawn on screen.
  Rect displayedRect(Size viewport, Size frame) {
    final fit = fittedRect(viewport, frame);
    return Rect.fromLTWH(fit.left * scale + offset.dx, fit.top * scale + offset.dy, fit.width * scale, fit.height * scale);
  }

  /// Viewport point -> normalised remote coordinates (clamped to [0,1]).
  Offset toRemote(Offset point, Size viewport, Size frame) {
    final rect = displayedRect(viewport, frame);
    if (rect.width <= 0 || rect.height <= 0) return const Offset(0.5, 0.5);
    return Offset(
      ((point.dx - rect.left) / rect.width).clamp(0.0, 1.0),
      ((point.dy - rect.top) / rect.height).clamp(0.0, 1.0),
    );
  }

  /// Applies a pinch: [focal] stays under the fingers while scaling by [factor]
  /// and moving by [pan]. The result is clamped so the frame stays visible.
  ViewTransform pinch({required Offset focal, required double factor, required Offset pan, required Size viewport, required Size frame}) {
    final newScale = (scale * factor).clamp(minScale, maxScale);
    final applied = newScale / scale;
    final newOffset = (offset - focal) * applied + focal + pan;
    return ViewTransform(scale: newScale, offset: newOffset).clamped(viewport, frame);
  }

  /// Keeps the zoomed frame covering the viewport (no empty borders when zoomed).
  ViewTransform clamped(Size viewport, Size frame) {
    if (scale <= minScale + 1e-6) return const ViewTransform();
    final fit = fittedRect(viewport, frame);
    final w = fit.width * scale;
    final h = fit.height * scale;
    double clampAxis(double value, double content, double fitStart, double view) {
      final start = fitStart * scale;
      if (content <= view) return (view - content) / 2 - start;
      return value.clamp(view - content - start, -start);
    }

    return ViewTransform(
      scale: scale,
      offset: Offset(clampAxis(offset.dx, w, fit.left, viewport.width), clampAxis(offset.dy, h, fit.top, viewport.height)),
    );
  }
}
