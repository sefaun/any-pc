import 'package:flutter/material.dart';

import '../app_controller.dart';
import 'app_scope.dart';

/// Asks for the AnyPC server on first launch (and when changing it).
class SetupScreen extends StatefulWidget {
  const SetupScreen({super.key, this.initial, this.popOnSave = false});

  final Uri? initial;
  final bool popOnSave;

  @override
  State<SetupScreen> createState() => _SetupScreenState();
}

class _SetupScreenState extends State<SetupScreen> {
  late final TextEditingController _address = TextEditingController(text: widget.initial?.toString() ?? '');
  String? _error;
  bool _busy = false;
  Uri? _failedUrl;

  Future<void> _connect({bool skipCheck = false}) async {
    final url = AppController.parseServer(_address.text);
    if (url == null) {
      setState(() => _error = 'Please enter a valid address, for example anypc.example.com.');
      return;
    }
    setState(() {
      _busy = true;
      _error = null;
      _address.text = url.toString();
    });
    try {
      if (!skipCheck) await AppController.verifyServer(url);
      if (!mounted) return;
      await AppScope.read(context).setServer(url);
      if (mounted && widget.popOnSave) Navigator.of(context).pop();
    } catch (e) {
      if (mounted) {
        setState(() {
          _error = 'Could not reach the AnyPC server: $e';
          _failedUrl = url;
        });
      }
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  @override
  void dispose() {
    _address.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Scaffold(
      appBar: widget.popOnSave ? AppBar(title: const Text('Server')) : null,
      body: SafeArea(
        child: Center(
          child: SingleChildScrollView(
            padding: const EdgeInsets.all(24),
            child: ConstrainedBox(
              constraints: const BoxConstraints(maxWidth: 480),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Icon(Icons.desktop_windows_rounded, size: 56, color: theme.colorScheme.primary),
                  const SizedBox(height: 16),
                  Text('Connect to your AnyPC server', style: theme.textTheme.headlineSmall, textAlign: TextAlign.center),
                  const SizedBox(height: 8),
                  Text(
                    'Enter the address of the AnyPC server. This phone and the computers you control must use the same server.',
                    style: theme.textTheme.bodyMedium?.copyWith(color: theme.colorScheme.onSurfaceVariant),
                    textAlign: TextAlign.center,
                  ),
                  const SizedBox(height: 24),
                  TextField(
                    controller: _address,
                    enabled: !_busy,
                    keyboardType: TextInputType.url,
                    autocorrect: false,
                    textInputAction: TextInputAction.go,
                    onSubmitted: (_) => _connect(),
                    decoration: const InputDecoration(labelText: 'Server address', hintText: 'anypc.example.com'),
                  ),
                  if (_error != null) ...[
                    const SizedBox(height: 12),
                    Text(_error!, style: TextStyle(color: theme.colorScheme.error)),
                  ],
                  const SizedBox(height: 20),
                  FilledButton(
                    onPressed: _busy ? null : _connect,
                    child: _busy
                        ? const SizedBox(width: 22, height: 22, child: CircularProgressIndicator(strokeWidth: 2.5))
                        : const Text('Connect'),
                  ),
                  if (_failedUrl != null && !_busy)
                    TextButton(onPressed: () => _connect(skipCheck: true), child: const Text('Save anyway')),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}
