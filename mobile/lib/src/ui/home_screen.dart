import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../app_controller.dart';
import '../core/device_id.dart';
import '../core/recent_connections.dart';
import '../net/signaling_client.dart';
import '../rtc/viewer_connection.dart';
import 'app_scope.dart';
import 'device_qr_sheet.dart';
import 'qr_scanner_screen.dart';
import 'setup_screen.dart';
import 'theme.dart';
import 'viewer_screen.dart';
import 'widgets/device_avatar.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  final _address = TextEditingController();
  final _password = TextEditingController();
  final List<StreamSubscription<dynamic>> _subscriptions = [];

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    if (_subscriptions.isNotEmpty) return;
    final controller = AppScope.read(context);
    _subscriptions.addAll([
      controller.messages.listen(_showMessage),
      controller.sessionStarts.listen(_openViewer),
      controller.sessionEnds.listen((end) {
        if (!end.byUser && mounted) {
          showDialog<void>(
            context: context,
            builder: (context) => AlertDialog(
              icon: const Icon(Icons.info_outline),
              title: const Text('Connection closed'),
              content: Text('The session with ${end.peer.name.isEmpty ? DeviceId.format(end.peer.id) : end.peer.name} has ended.\n\n${end.reason}'),
              actions: [TextButton(onPressed: () => Navigator.of(context).pop(), child: const Text('OK'))],
            ),
          );
        }
      }),
    ]);
  }

  void _showMessage(String text) {
    if (!mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(text)));
  }

  void _openViewer(ViewerConnection connection) {
    if (!mounted) return;
    Navigator.of(context).push(MaterialPageRoute<void>(builder: (_) => ViewerScreen(connection: connection)));
  }

  void _connect([String? id]) {
    final target = id ?? DeviceId.normalize(_address.text);
    if (!DeviceId.isValid(target)) {
      _showMessage('Please enter a valid 9 digit address.');
      return;
    }
    FocusScope.of(context).unfocus();
    _address.text = DeviceId.format(target);
    AppScope.read(context).connectTo(target, password: id == null ? _password.text : '');
    _password.clear();
  }

  Future<void> _scanQr() async {
    FocusScope.of(context).unfocus();
    final id = await QrScannerScreen.scan(context);
    if (id == null || !mounted) return;
    // A scanned code is an explicit choice: send the connection request right away
    // (with the password field's content, if any).
    _address.text = DeviceId.format(id);
    _connect();
  }

  @override
  void dispose() {
    for (final s in _subscriptions) {
      s.cancel();
    }
    _address.dispose();
    _password.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final controller = AppScope.of(context);
    return Scaffold(
      appBar: AppBar(
        title: const Text('AnyPC'),
        actions: [
          _StatusDot(state: controller.signalingState),
          PopupMenuButton<String>(
            onSelected: (value) => _onMenu(value, controller),
            itemBuilder: (context) => const [
              PopupMenuItem(value: 'qr', child: Text('My QR code')),
              PopupMenuItem(value: 'newId', child: Text('Get a new address')),
              PopupMenuItem(value: 'server', child: Text('Change server')),
              PopupMenuItem(value: 'name', child: Text('Device name')),
              PopupMenuItem(value: 'about', child: Text('About')),
            ],
          ),
        ],
      ),
      body: SafeArea(
        child: ListView(
          padding: const EdgeInsets.all(16),
          children: [
            _ConnectCard(
              address: _address,
              password: _password,
              controller: controller,
              onConnect: () => _connect(),
              onScan: _scanQr,
            ),
            const SizedBox(height: 12),
            _ThisDevice(controller: controller),
            const SizedBox(height: 20),
            _RecentList(controller: controller, onConnect: _connect, onMessage: _showMessage),
          ],
        ),
      ),
    );
  }

  Future<void> _onMenu(String value, AppController controller) async {
    switch (value) {
      case 'qr':
        final id = controller.deviceId;
        if (id == null) {
          _showMessage('This phone has no address yet.');
        } else {
          await showDeviceQrSheet(context, deviceId: id, title: 'This phone');
        }
      case 'newId':
        final current = controller.deviceId;
        if (current == null || !controller.isOnline) {
          _showMessage('Connect to the server first.');
          return;
        }
        final ok = await showDialog<bool>(
          context: context,
          builder: (context) => AlertDialog(
            title: const Text('Get a new address'),
            content: Text('Give this phone a new random address?\n\n'
                'The current address ${DeviceId.format(current)} stops working right away.'),
            actions: [
              TextButton(onPressed: () => Navigator.pop(context, false), child: const Text('Cancel')),
              FilledButton(onPressed: () => Navigator.pop(context, true), child: const Text('New address')),
            ],
          ),
        );
        if (ok == true) await controller.changeDeviceId();
      case 'server':
        await Navigator.of(context).push(
          MaterialPageRoute<void>(builder: (_) => SetupScreen(initial: controller.serverUrl, popOnSave: true)),
        );
      case 'name':
        final name = await _askText(context, 'Device name', controller.settings.deviceName);
        if (name != null && name.trim().isNotEmpty) await controller.setDeviceName(name);
      case 'about':
        showAboutDialog(
          context: context,
          applicationName: 'AnyPC',
          applicationVersion: appVersion,
          children: const [Text('Control your computers from your phone. Built with Flutter and Cloudflare Realtime.')],
        );
    }
  }
}

Future<String?> _askText(BuildContext context, String title, String initial, {String? hint}) {
  final field = TextEditingController(text: initial);
  return showDialog<String>(
    context: context,
    builder: (context) => AlertDialog(
      title: Text(title),
      content: TextField(controller: field, autofocus: true, maxLength: 64, decoration: InputDecoration(hintText: hint)),
      actions: [
        TextButton(onPressed: () => Navigator.of(context).pop(), child: const Text('Cancel')),
        FilledButton(onPressed: () => Navigator.of(context).pop(field.text), child: const Text('Save')),
      ],
    ),
  ).whenComplete(field.dispose);
}

class _StatusDot extends StatelessWidget {
  const _StatusDot({required this.state});
  final SignalingState state;

  @override
  Widget build(BuildContext context) {
    final (color, label) = switch (state) {
      SignalingState.connected => (const Color(0xFF30A46C), 'Online'),
      SignalingState.connecting => (const Color(0xFFF5A524), 'Connecting…'),
      SignalingState.disconnected => (const Color(0xFFE5484D), 'Offline'),
    };
    return Tooltip(
      message: label,
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 8),
        child: Icon(Icons.circle, size: 12, color: color),
      ),
    );
  }
}

class _ConnectCard extends StatelessWidget {
  const _ConnectCard({
    required this.address,
    required this.password,
    required this.controller,
    required this.onConnect,
    required this.onScan,
  });

  final TextEditingController address;
  final TextEditingController password;
  final AppController controller;
  final VoidCallback onConnect;
  final VoidCallback onScan;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final busy = controller.outgoing != OutgoingState.idle;
    final target = controller.outgoingTarget;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Text('Remote computer', style: theme.textTheme.titleMedium),
            const SizedBox(height: 4),
            Text('Enter the address of the computer you want to control, or scan its QR code.',
                style: theme.textTheme.bodySmall?.copyWith(color: theme.colorScheme.onSurfaceVariant)),
            const SizedBox(height: 12),
            TextField(
              controller: address,
              enabled: !busy,
              keyboardType: TextInputType.number,
              inputFormatters: [FilteringTextInputFormatter.allow(RegExp(r'[0-9 ]')), LengthLimitingTextInputFormatter(11)],
              style: theme.textTheme.titleLarge,
              decoration: InputDecoration(
                hintText: '123 456 789',
                prefixIcon: const Icon(Icons.computer),
                suffixIcon: IconButton(
                  tooltip: 'Scan QR code',
                  icon: const Icon(Icons.qr_code_scanner),
                  onPressed: busy ? null : onScan,
                ),
              ),
              onSubmitted: (_) => onConnect(),
            ),
            const SizedBox(height: 10),
            TextField(
              controller: password,
              enabled: !busy,
              obscureText: true,
              decoration: const InputDecoration(labelText: 'Password (optional, for unattended access)', isDense: true),
              onSubmitted: (_) => onConnect(),
            ),
            const SizedBox(height: 14),
            if (!busy)
              FilledButton.icon(
                onPressed: controller.isOnline ? onConnect : null,
                icon: const Icon(Icons.link),
                label: const Text('Connect'),
              )
            else
              Row(
                children: [
                  const SizedBox(width: 20, height: 20, child: CircularProgressIndicator(strokeWidth: 2.5)),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Text(controller.outgoing == OutgoingState.waiting
                        ? 'Waiting for ${target?.name.isNotEmpty == true ? target!.name : 'the remote side'} to accept…'
                        : 'Requesting connection…'),
                  ),
                  TextButton(onPressed: controller.cancelOutgoing, child: const Text('Cancel')),
                ],
              ),
          ],
        ),
      ),
    );
  }
}

class _ThisDevice extends StatelessWidget {
  const _ThisDevice({required this.controller});
  final AppController controller;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final id = controller.deviceId;
    final error = controller.lastError;
    final text = Text(
      error != null
          ? 'Server: $error'
          : 'This phone: ${id == null ? '—' : DeviceId.format(id)} · ${controller.serverUrl?.host ?? ''}',
      style: theme.textTheme.bodySmall?.copyWith(color: error != null ? theme.colorScheme.error : theme.colorScheme.onSurfaceVariant),
      textAlign: TextAlign.center,
    );
    if (error != null || id == null) return text;
    return Center(
      child: TextButton.icon(
        style: TextButton.styleFrom(foregroundColor: theme.colorScheme.onSurfaceVariant),
        icon: const Icon(Icons.qr_code_2, size: 18),
        label: text,
        onPressed: () => showDeviceQrSheet(context, deviceId: id, title: 'This phone'),
      ),
    );
  }
}

class _RecentList extends StatelessWidget {
  const _RecentList({required this.controller, required this.onConnect, required this.onMessage});

  final AppController controller;
  final void Function(String id) onConnect;
  final void Function(String text) onMessage;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final entries = controller.recent.entries;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Text('Recent sessions', style: theme.textTheme.titleMedium),
        const SizedBox(height: 8),
        if (entries.isEmpty)
          Padding(
            padding: const EdgeInsets.symmetric(vertical: 24),
            child: Text('Computers you connect to will appear here.',
                textAlign: TextAlign.center, style: TextStyle(color: theme.colorScheme.onSurfaceVariant)),
          )
        else
          for (final entry in entries)
            Card(
              margin: const EdgeInsets.only(bottom: 8),
              child: ListTile(
                leading: DeviceAvatar(name: entry.displayName, platform: entry.platform),
                title: Text(entry.displayName, maxLines: 1, overflow: TextOverflow.ellipsis),
                subtitle: Text([
                  DeviceId.format(entry.id),
                  if (AppTheme.platformLabel(entry.platform).isNotEmpty) AppTheme.platformLabel(entry.platform),
                  _relative(entry.lastConnected),
                ].join(' · ')),
                trailing: IconButton(icon: const Icon(Icons.more_vert), onPressed: () => _showActions(context, entry)),
                onTap: () => onConnect(entry.id),
                onLongPress: () => _showActions(context, entry),
              ),
            ),
      ],
    );
  }

  Future<void> _showActions(BuildContext context, RecentConnection entry) async {
    final action = await showModalBottomSheet<String>(
      context: context,
      showDragHandle: true,
      builder: (context) => SafeArea(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            ListTile(title: Text(entry.displayName), subtitle: Text(DeviceId.format(entry.id))),
            ListTile(leading: const Icon(Icons.link), title: const Text('Connect'), onTap: () => Navigator.pop(context, 'connect')),
            ListTile(leading: const Icon(Icons.copy), title: const Text('Copy address'), onTap: () => Navigator.pop(context, 'copy')),
            ListTile(leading: const Icon(Icons.qr_code_2), title: const Text('Show QR code'), onTap: () => Navigator.pop(context, 'qr')),
            ListTile(leading: const Icon(Icons.edit), title: const Text('Rename'), onTap: () => Navigator.pop(context, 'rename')),
            ListTile(
              leading: const Icon(Icons.delete_outline),
              title: const Text('Remove from list'),
              onTap: () => Navigator.pop(context, 'remove'),
            ),
          ],
        ),
      ),
    );
    if (!context.mounted) return;
    switch (action) {
      case 'connect':
        onConnect(entry.id);
      case 'copy':
        await Clipboard.setData(ClipboardData(text: entry.id));
        onMessage('Address copied.');
      case 'qr':
        await showDeviceQrSheet(context, deviceId: entry.id, title: entry.displayName);
      case 'rename':
        final alias = await _askText(context, 'Rename', entry.alias, hint: entry.name.isEmpty ? null : entry.name);
        if (alias != null) controller.renameRecent(entry.id, alias);
      case 'remove':
        controller.removeRecent(entry.id);
    }
  }

  static String _relative(DateTime? when) {
    if (when == null) return 'never';
    final diff = DateTime.now().difference(when);
    if (diff.inMinutes < 1) return 'just now';
    if (diff.inHours < 1) return '${diff.inMinutes} min ago';
    if (diff.inDays < 1) return '${diff.inHours} h ago';
    if (diff.inDays < 7) return '${diff.inDays} d ago';
    return '${when.day}.${when.month}.${when.year}';
  }
}
