/// Physical key codes (DOM `KeyboardEvent.code`) understood by the desktop host.
abstract final class KeyCodes {
  static const escape = 'Escape';
  static const tab = 'Tab';
  static const enter = 'Enter';
  static const backspace = 'Backspace';
  static const delete = 'Delete';
  static const arrowLeft = 'ArrowLeft';
  static const arrowRight = 'ArrowRight';
  static const arrowUp = 'ArrowUp';
  static const arrowDown = 'ArrowDown';
  static const home = 'Home';
  static const end = 'End';
  static const pageUp = 'PageUp';
  static const pageDown = 'PageDown';
  static const controlLeft = 'ControlLeft';
  static const altLeft = 'AltLeft';
  static const shiftLeft = 'ShiftLeft';
  static const metaLeft = 'MetaLeft';

  static String function(int n) => 'F$n';

  /// Physical key for an ASCII letter or digit (used for shortcuts such as Ctrl+C).
  static String? forCharacter(String char) {
    if (char.length != 1) return null;
    final c = char.toUpperCase().codeUnitAt(0);
    if (c >= 0x41 && c <= 0x5A) return 'Key${String.fromCharCode(c)}';
    if (c >= 0x30 && c <= 0x39) return 'Digit${String.fromCharCode(c)}';
    if (char == ' ') return 'Space';
    return null;
  }
}

/// Result of diffing the soft keyboard buffer against its sentinel.
class KeyboardDelta {
  const KeyboardDelta({this.backspaces = 0, this.inserted = ''});
  final int backspaces;
  final String inserted;
  bool get isEmpty => backspaces == 0 && inserted.isEmpty;
}

/// Soft keyboards report text, not key presses. The input field always holds
/// [sentinel]; anything typed after it is new text and removing characters
/// (including the sentinel) means Backspace.
abstract final class SoftKeyboard {
  static const sentinel = '​​';

  static KeyboardDelta diff(String value) {
    if (value == sentinel) return const KeyboardDelta();
    if (value.startsWith(sentinel)) return KeyboardDelta(inserted: value.substring(sentinel.length));
    var common = 0;
    while (common < value.length && common < sentinel.length && value[common] == sentinel[common]) {
      common++;
    }
    final removed = sentinel.length - common;
    return KeyboardDelta(backspaces: removed, inserted: value.substring(common).replaceAll('​', ''));
  }
}
