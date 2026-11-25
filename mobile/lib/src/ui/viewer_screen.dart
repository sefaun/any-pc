import 'dart:async';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:flutter_webrtc/flutter_webrtc.dart';

import '../input/keyboard.dart';
import '../input/touch_controller.dart';
import '../input/view_transform.dart';
import '../rtc/file_transfer.dart';
import '../rtc/viewer_connection.dart';
import 'app_scope.dart';
import 'transfers_sheet.dart';

/// Full screen remote desktop view with touch, keyboard and file controls.
class ViewerScreen extends StatefulWidget {
  const ViewerScreen({super.key, required this.connection});
  final ViewerConnection connection;

  @override
  State<ViewerScreen> createState() => _ViewerScreenState();
}

class _ViewerScreenState extends State<ViewerScreen> {
  late final TouchController _touch = TouchController(
    sink: widget.connection,
    onTransformChanged: (_) => setState(() {}),
  );
  final _keyboardField = TextEditingController(text: SoftKeyboard.sentinel);
  final _keyboardFocus = FocusNode();
  final Set<String> _modifiers = {};
  bool _keyboardOpen = false;
  bool _closing = false;

  ViewerConnection get _connection => widget.connection;

  @override
  void initState() {
    super.initState();
    _connection.addListener(_onConnectionChanged);
    _keyboardFocus.addListener(() => setState(() => _keyboardOpen = _keyboardFocus.hasFocus));
  }

  void _onConnectionChanged() {
    if (!mounted) return;
    if (_connection.state == ViewerState.closed || _connection.state == ViewerState.failed) {
      _leave();
      return;
    }
    setState(() {});
  }

  /// Ends the session at the user's request and leaves the screen right away;
  /// the connection is torn down in the background.
  Future<void> _close() async {
    if (_closing) return;
    final controller = AppScope.read(context);
    _leave();
    await controller.endSession(byUser: true);
  }

  /// Removes this screen (and any dialog or sheet opened on top of it).
  ///
  /// Uses pop() rather than maybePop(): the PopScope below blocks maybePop()
  /// while a session is active, which used to leave this screen open after a
  /// failed connection or a confirmed Disconnect.
  void _leave() {
    if (!mounted) return;
    _keyboardFocus.unfocus();
    if (!_closing) setState(() => _closing = true);
    final route = ModalRoute.of(context);
    final navigator = Navigator.of(context);
    if (route == null || !route.isActive) return;
    if (!route.isCurrent) navigator.popUntil((r) => r == route);
    navigator.pop();
  }

  @override
  void dispose() {
    _connection.removeListener(_onConnectionChanged);
    _touch.dispose();
    _keyboardField.dispose();
    _keyboardFocus.dispose();
    super.dispose();
  }

  // ---- Keyboard ------------------------------------------------------------------------------

  void _toggleKeyboard() {
    if (_keyboardFocus.hasFocus) {
      _keyboardFocus.unfocus();
    } else {
      _keyboardFocus.requestFocus();
    }
  }

  void _onKeyboardChanged(String value) {
    final delta = SoftKeyboard.diff(value);
    _keyboardField.value = const TextEditingValue(
      text: SoftKeyboard.sentinel,
      selection: TextSelection.collapsed(offset: SoftKeyboard.sentinel.length),
    );
    if (delta.isEmpty) return;
    for (var i = 0; i < delta.backspaces; i++) {
      _tapKey(KeyCodes.backspace);
    }
    final text = delta.inserted;
    if (text.isEmpty) return;
    if (_modifiers.isNotEmpty && text.length == 1 && KeyCodes.forCharacter(text) != null) {
      _tapKey(KeyCodes.forCharacter(text)!);
      return;
    }
    // Send text in runs, turning newlines into Enter key presses.
    final parts = text.split('\n');
    for (var i = 0; i < parts.length; i++) {
      if (parts[i].isNotEmpty) _connection.text(parts[i]);
      if (i < parts.length - 1) _tapKey(KeyCodes.enter);
    }
  }

  /// Presses a key together with the active sticky modifiers, then releases them.
  void _tapKey(String code, {Set<String> extraModifiers = const {}}) {
    final modifiers = {..._modifiers, ...extraModifiers};
    for (final m in modifiers) {
      _connection.key(m, true);
    }
    _connection.key(code, true);
    _connection.key(code, false);
    for (final m in modifiers.toList().reversed) {
      _connection.key(m, false);
    }
    if (_modifiers.isNotEmpty) setState(_modifiers.clear);
  }

  void _toggleModifier(String code) => setState(() => _modifiers.contains(code) ? _modifiers.remove(code) : _modifiers.add(code));

  // ---- Commands ------------------------------------------------------------------------------

  Future<void> _sendFiles() async {
    final picked = await FilePicker.pickFiles(dialogTitle: 'Send files to ${_connection.peer.name}');
    for (final file in picked) {
      final xfile = file.xFile;
      _connection.sendFile(OutgoingFile(name: file.name, size: await xfile.length(), open: xfile.openRead));
    }
    if (picked.isNotEmpty && mounted) _showTransfers();
  }

  void _showTransfers() {
    final service = _connection.files;
    if (service == null) return;
    showModalBottomSheet<void>(
      context: context,
      showDragHandle: true,
      isScrollControlled: true,
      builder: (_) => TransfersSheet(service: service, onSend: _sendFiles),
    );
  }

  Future<void> _sendClipboard() async {
    final ok = await _connection.sendClipboard();
    if (mounted) {
      ScaffoldMessenger.of(context)
          .showSnackBar(SnackBar(content: Text(ok ? 'Clipboard sent' : 'The clipboard is empty or too large')));
    }
  }

  // ---- UI ------------------------------------------------------------------------------------

  @override
  Widget build(BuildContext context) {
    final connection = _connection;
    return PopScope(
      canPop: _closing,
      onPopInvokedWithResult: (didPop, _) {
        if (!didPop) _confirmClose();
      },
      child: Scaffold(
        backgroundColor: Colors.black,
        resizeToAvoidBottomInset: false,
        body: Stack(
          children: [
            Positioned.fill(child: _buildRemoteView()),
            if (connection.state != ViewerState.connected) _buildStatusOverlay(),
            Positioned(left: 0, bottom: 0, width: 1, height: 1, child: _buildKeyboardSink()),
            Positioned(top: 0, left: 0, right: 0, child: SafeArea(bottom: false, child: _buildToolbar())),
            if (_keyboardOpen)
              Positioned(
                left: 0,
                right: 0,
                bottom: MediaQuery.of(context).viewInsets.bottom,
                child: _buildSpecialKeys(),
              ),
          ],
        ),
      ),
    );
  }

  Widget _buildRemoteView() {
    return LayoutBuilder(builder: (context, constraints) {
      final viewport = Size(constraints.maxWidth, constraints.maxHeight);
      final frame = _connection.frameSize.isEmpty ? viewport : _connection.frameSize;
      _touch
        ..viewport = viewport
        ..frame = frame;
      final rect = _touch.transform.displayedRect(viewport, frame);
      return Listener(
        behavior: HitTestBehavior.opaque,
        onPointerDown: (e) => _touch.pointerDown(e.pointer, e.localPosition),
        onPointerMove: (e) => _touch.pointerMove(e.pointer, e.localPosition),
        onPointerUp: (e) => _touch.pointerUp(e.pointer),
        onPointerCancel: (e) => _touch.pointerCancel(e.pointer),
        child: Stack(
          children: [
            Positioned.fromRect(
              rect: rect,
              child: RTCVideoView(_connection.renderer, objectFit: RTCVideoViewObjectFit.RTCVideoViewObjectFitContain),
            ),
          ],
        ),
      );
    });
  }

  Widget _buildStatusOverlay() {
    final text = switch (_connection.state) {
      ViewerState.waiting => 'Waiting for ${_connection.peer.name}…',
      ViewerState.connecting => 'Connecting…',
      ViewerState.failed => _connection.error ?? 'Connection failed',
      _ => '',
    };
    return Positioned.fill(
      child: IgnorePointer(
        child: ColoredBox(
          color: Colors.black54,
          child: Center(
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                const CircularProgressIndicator(),
                const SizedBox(height: 16),
                Text(text, style: const TextStyle(color: Colors.white)),
              ],
            ),
          ),
        ),
      ),
    );
  }

  Widget _buildKeyboardSink() {
    return Opacity(
      opacity: 0,
      child: TextField(
        controller: _keyboardField,
        focusNode: _keyboardFocus,
        autocorrect: false,
        enableSuggestions: false,
        keyboardType: TextInputType.multiline,
        maxLines: null,
        onChanged: _onKeyboardChanged,
      ),
    );
  }

  Widget _buildToolbar() {
    final connection = _connection;
    final transfers = connection.files?.transfers ?? const <FileTransfer>[];
    final active = transfers.where((t) => t.state == TransferState.transferring).length;
    return Container(
      margin: const EdgeInsets.all(8),
      padding: const EdgeInsets.symmetric(horizontal: 4),
      decoration: BoxDecoration(color: Colors.black.withValues(alpha: 0.6), borderRadius: BorderRadius.circular(12)),
      child: SingleChildScrollView(
        scrollDirection: Axis.horizontal,
        child: Row(
          children: [
            _ToolButton(icon: Icons.close, tooltip: 'Disconnect', onPressed: _confirmClose),
            _ToolButton(
              icon: _keyboardOpen ? Icons.keyboard_hide : Icons.keyboard,
              tooltip: 'Keyboard',
              onPressed: connection.inputSupported ? _toggleKeyboard : null,
            ),
            _ToolButton(
              icon: Icons.swipe_vertical,
              tooltip: _touch.scrollMode ? 'Scroll mode on' : 'Scroll mode off',
              selected: _touch.scrollMode,
              onPressed: () => setState(() => _touch.scrollMode = !_touch.scrollMode),
            ),
            // The person at the computer turns its sound on and off; this only shows the state.
            if (connection.audioAvailable)
              Tooltip(
                message: connection.audioEnabled ? 'Sound on' : 'Sound turned off on the computer',
                child: Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 8),
                  child: Icon(connection.audioEnabled ? Icons.volume_up : Icons.volume_off, color: Colors.white70),
                ),
              ),
            if (_touch.transform.scale > 1.01)
              _ToolButton(icon: Icons.zoom_out_map, tooltip: 'Reset zoom', onPressed: _touch.resetZoom),
            if (connection.screens.length > 1)
              PopupMenuButton<int>(
                tooltip: 'Remote screen',
                icon: const Icon(Icons.monitor, color: Colors.white),
                onSelected: connection.requestScreen,
                itemBuilder: (context) => [
                  for (final s in connection.screens)
                    CheckedPopupMenuItem(
                      value: s.index,
                      checked: s.index == connection.currentScreen,
                      child: Text(s.name.isEmpty ? 'Screen ${s.index + 1}' : s.name),
                    ),
                ],
              ),
            _ToolButton(icon: Icons.upload_file, tooltip: 'Send files', onPressed: connection.files == null ? null : _sendFiles),
            Badge(
              isLabelVisible: active > 0,
              label: Text('$active'),
              child: _ToolButton(icon: Icons.swap_vert, tooltip: 'Transfers', onPressed: connection.files == null ? null : _showTransfers),
            ),
            _ToolButton(icon: Icons.content_paste_go, tooltip: 'Send clipboard', onPressed: _sendClipboard),
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 8),
              child: Text(connection.peer.name, style: const TextStyle(color: Colors.white70)),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildSpecialKeys() {
    Widget key(String label, String code) => _KeyChip(label: label, onPressed: () => _tapKey(code));
    Widget modifier(String label, String code) =>
        _KeyChip(label: label, selected: _modifiers.contains(code), onPressed: () => _toggleModifier(code));
    final isMacHost = _connection.peer.platform == 'macos';
    return Container(
      color: Colors.black87,
      padding: const EdgeInsets.symmetric(vertical: 6),
      child: SingleChildScrollView(
        scrollDirection: Axis.horizontal,
        padding: const EdgeInsets.symmetric(horizontal: 8),
        child: Row(
          children: [
            key('Esc', KeyCodes.escape),
            key('Tab', KeyCodes.tab),
            modifier('Ctrl', KeyCodes.controlLeft),
            modifier('Alt', KeyCodes.altLeft),
            modifier('Shift', KeyCodes.shiftLeft),
            modifier(isMacHost ? '⌘' : 'Win', KeyCodes.metaLeft),
            key('←', KeyCodes.arrowLeft),
            key('↑', KeyCodes.arrowUp),
            key('↓', KeyCodes.arrowDown),
            key('→', KeyCodes.arrowRight),
            key('Home', KeyCodes.home),
            key('End', KeyCodes.end),
            key('PgUp', KeyCodes.pageUp),
            key('PgDn', KeyCodes.pageDown),
            key('Del', KeyCodes.delete),
            for (var i = 1; i <= 12; i++) key('F$i', KeyCodes.function(i)),
            _KeyChip(
              label: 'Ctrl+Alt+Del',
              onPressed: () => _tapKey(KeyCodes.delete, extraModifiers: {KeyCodes.controlLeft, KeyCodes.altLeft}),
            ),
          ],
        ),
      ),
    );
  }

  Future<void> _confirmClose() async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Disconnect?'),
        content: Text('End the session with ${_connection.peer.name}?'),
        actions: [
          TextButton(onPressed: () => Navigator.pop(context, false), child: const Text('Cancel')),
          FilledButton(onPressed: () => Navigator.pop(context, true), child: const Text('Disconnect')),
        ],
      ),
    );
    if (ok == true) await _close();
  }
}

class _ToolButton extends StatelessWidget {
  const _ToolButton({required this.icon, required this.tooltip, this.onPressed, this.selected = false});
  final IconData icon;
  final String tooltip;
  final VoidCallback? onPressed;
  final bool selected;

  @override
  Widget build(BuildContext context) => IconButton(
        icon: Icon(icon),
        tooltip: tooltip,
        color: selected ? Theme.of(context).colorScheme.primary : Colors.white,
        disabledColor: Colors.white30,
        onPressed: onPressed,
      );
}

class _KeyChip extends StatelessWidget {
  const _KeyChip({required this.label, required this.onPressed, this.selected = false});
  final String label;
  final VoidCallback onPressed;
  final bool selected;

  @override
  Widget build(BuildContext context) => Padding(
        padding: const EdgeInsets.symmetric(horizontal: 3),
        child: ChoiceChip(label: Text(label), selected: selected, onSelected: (_) => onPressed(), showCheckmark: false),
      );
}

// Keeps ViewTransform referenced for readers of this file.
typedef RemoteViewTransform = ViewTransform;
