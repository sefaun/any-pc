import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:qr_flutter/qr_flutter.dart';

import '../core/device_id.dart';

/// Bottom sheet that shows a device address as text and as a QR code.
Future<void> showDeviceQrSheet(BuildContext context, {required String deviceId, required String title}) {
  return showModalBottomSheet<void>(
    context: context,
    showDragHandle: true,
    isScrollControlled: true,
    builder: (context) => DeviceQrView(deviceId: deviceId, title: title),
  );
}

class DeviceQrView extends StatelessWidget {
  const DeviceQrView({super.key, required this.deviceId, required this.title});

  final String deviceId;
  final String title;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return SafeArea(
      child: Padding(
        padding: const EdgeInsets.fromLTRB(24, 0, 24, 24),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Text(title, style: theme.textTheme.titleMedium),
            const SizedBox(height: 16),
            Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(color: Colors.white, borderRadius: BorderRadius.circular(12)),
              child: QrImageView(
                data: DeviceId.qrPayload(deviceId),
                size: 220,
                backgroundColor: Colors.white,
                semanticsLabel: 'QR code of ${DeviceId.format(deviceId)}',
              ),
            ),
            const SizedBox(height: 16),
            SelectableText(
              DeviceId.format(deviceId),
              style: theme.textTheme.headlineSmall?.copyWith(fontWeight: FontWeight.w700, letterSpacing: 1),
            ),
            const SizedBox(height: 8),
            TextButton.icon(
              icon: const Icon(Icons.copy),
              label: const Text('Copy address'),
              onPressed: () async {
                await Clipboard.setData(ClipboardData(text: deviceId));
                if (context.mounted) Navigator.of(context).pop();
              },
            ),
          ],
        ),
      ),
    );
  }
}
