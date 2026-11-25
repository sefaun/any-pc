import 'package:flutter/material.dart';

abstract final class AppTheme {
  static const accent = Color(0xFFE5484D);

  static ThemeData light() => _build(Brightness.light);
  static ThemeData dark() => _build(Brightness.dark);

  static ThemeData _build(Brightness brightness) {
    final scheme = ColorScheme.fromSeed(seedColor: accent, brightness: brightness, primary: accent);
    return ThemeData(
      colorScheme: scheme,
      useMaterial3: true,
      inputDecorationTheme: const InputDecorationTheme(border: OutlineInputBorder()),
      filledButtonTheme: FilledButtonThemeData(
        style: FilledButton.styleFrom(minimumSize: const Size(0, 48), textStyle: const TextStyle(fontWeight: FontWeight.w600)),
      ),
    );
  }

  static Color platformColor(String platform) => switch (platform) {
        'windows' => const Color(0xFF0078D4),
        'macos' => const Color(0xFF63636B),
        'linux' => const Color(0xFFE95420),
        'android' => const Color(0xFF3DDC84),
        'ios' => const Color(0xFF8E8E93),
        _ => accent,
      };

  static String platformLabel(String platform) => switch (platform) {
        'windows' => 'Windows',
        'macos' => 'macOS',
        'linux' => 'Linux',
        'android' => 'Android',
        'ios' => 'iOS',
        _ => '',
      };
}
