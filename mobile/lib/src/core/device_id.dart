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
}
