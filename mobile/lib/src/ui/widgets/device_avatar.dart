import 'package:flutter/material.dart';

import '../theme.dart';

class DeviceAvatar extends StatelessWidget {
  const DeviceAvatar({super.key, required this.name, required this.platform, this.size = 44});

  final String name;
  final String platform;
  final double size;

  @override
  Widget build(BuildContext context) {
    final color = AppTheme.platformColor(platform);
    return Container(
      width: size,
      height: size * 0.8,
      decoration: BoxDecoration(color: color, borderRadius: BorderRadius.circular(8)),
      alignment: Alignment.center,
      child: Text(
        name.isEmpty ? '?' : name.characters.first.toUpperCase(),
        style: TextStyle(color: Colors.white, fontWeight: FontWeight.bold, fontSize: size * 0.4),
      ),
    );
  }
}
