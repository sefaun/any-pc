/// Helpers for the 9 digit device addresses used by AnyPC.
abstract final class DeviceId {
  static final RegExp _valid = RegExp(r'^[1-9]\d{8}$');

  /// Removes everything that is not a digit ("482 913-507" -> "482913507").
  static String normalize(String input) => input.replaceAll(RegExp(r'\D'), '');

  static bool isValid(String normalized) => _valid.hasMatch(normalized);

  /// Groups digits for display ("482913507" -> "482 913 507").
  static String format(String normalized) {
    if (normalized.length != 9) return normalized;
    return '${normalized.substring(0, 3)} ${normalized.substring(3, 6)} ${normalized.substring(6)}';
  }

  static const _qrPrefix = 'anypc://connect?id=';

  /// Content of the QR code that shares an address ("anypc://connect?id=482913507").
  static String qrPayload(String normalized) => '$_qrPrefix$normalized';

  /// Extracts an address from a scanned QR code: an anypc:// link or a plain
  /// (optionally grouped) address. Returns null when the code is not an address.
  static String? fromQrPayload(String text) {
    final trimmed = text.trim();
    String candidate;
    final uri = Uri.tryParse(trimmed);
    if (uri != null && uri.scheme.toLowerCase() == 'anypc') {
      candidate = normalize(uri.queryParameters['id'] ?? '');
    } else if (RegExp(r'^[0-9 -]+$').hasMatch(trimmed)) {
      candidate = normalize(trimmed);
    } else {
      return null;
    }
    return isValid(candidate) ? candidate : null;
  }
}
