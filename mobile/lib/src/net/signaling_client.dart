import 'dart:async';
import 'dart:convert';
import 'dart:math';

import 'package:web_socket_channel/io.dart';

import '../core/server_url.dart';
import 'protocol.dart';

enum SignalingState { disconnected, connecting, connected }

/// WebSocket client for the signaling gateway with automatic reconnection.
class SignalingClient {
  final _frames = StreamController<ServerFrame>.broadcast();
  final _states = StreamController<SignalingState>.broadcast();
  final _authFailures = StreamController<void>.broadcast();
  final _random = Random();

  IOWebSocketChannel? _channel;
  StreamSubscription<dynamic>? _subscription;
  Timer? _reconnectTimer;
  Uri? _serverUrl;
  String? _token;
  bool _shouldConnect = false;
  int _attempt = 0;
  SignalingState _state = SignalingState.disconnected;

  Stream<ServerFrame> get frames => _frames.stream;
  Stream<SignalingState> get states => _states.stream;
  Stream<void> get authenticationFailures => _authFailures.stream;
  SignalingState get state => _state;
  bool get isConnected => _state == SignalingState.connected;

  void connect(Uri serverUrl, String token) {
    _serverUrl = serverUrl;
    _token = token;
    _shouldConnect = true;
    _attempt = 0;
    _reconnectTimer?.cancel();
    _close();
    _open();
  }

  void updateToken(String token) => _token = token;

  void disconnect() {
    _shouldConnect = false;
    _reconnectTimer?.cancel();
    _close();
    _setState(SignalingState.disconnected);
  }

  void send(String event, Map<String, Object?> data) {
    final channel = _channel;
    if (channel == null || !isConnected) return;
    channel.sink.add(jsonEncode({'event': event, 'data': data}));
  }

  void _open() {
    final base = _serverUrl;
    final token = _token;
    if (!_shouldConnect || base == null || token == null) return;
    final http = joinUrl(base, '/ws');
    final url = http.replace(scheme: http.scheme == 'https' ? 'wss' : 'ws');
    _setState(SignalingState.connecting);
    final channel = IOWebSocketChannel.connect(
      url,
      headers: {'Authorization': 'Bearer $token'},
      pingInterval: const Duration(seconds: 20),
      connectTimeout: const Duration(seconds: 15),
    );
    _channel = channel;
    _subscription = channel.stream.listen(
      _onMessage,
      onError: (_) => _onClosed(channel),
      onDone: () => _onClosed(channel),
      cancelOnError: true,
    );
  }

  void _onMessage(dynamic message) {
    if (message is! String) return;
    Object? decoded;
    try {
      decoded = jsonDecode(message);
    } on FormatException {
      return;
    }
    if (decoded is! Map) return;
    final event = decoded['event'];
    final data = decoded['data'];
    if (event is! String) return;
    final frame = ServerFrame(event, data is Map<String, dynamic> ? data : <String, dynamic>{});
    if (event == ServerEvent.sessionReady) {
      _attempt = 0;
      _setState(SignalingState.connected);
    }
    _frames.add(frame);
  }

  void _onClosed(IOWebSocketChannel channel) {
    if (!identical(channel, _channel)) return;
    final code = channel.closeCode;
    _channel = null;
    _subscription = null;
    _setState(SignalingState.disconnected);
    if (code == CloseCode.unauthorized) {
      _authFailures.add(null);
      return;
    }
    if (code == CloseCode.replaced) {
      _shouldConnect = false;
      return;
    }
    _scheduleReconnect();
  }

  void _scheduleReconnect() {
    if (!_shouldConnect || (_reconnectTimer?.isActive ?? false)) return;
    final exp = min(_attempt++, 5);
    final base = min(1000 * (1 << exp), 30000);
    _reconnectTimer = Timer(Duration(milliseconds: base + _random.nextInt(base ~/ 4 + 1)), _open);
  }

  void _close() {
    final channel = _channel;
    _channel = null;
    _subscription?.cancel();
    _subscription = null;
    channel?.sink.close();
  }

  void _setState(SignalingState state) {
    if (_state == state) return;
    _state = state;
    _states.add(state);
  }

  void dispose() {
    disconnect();
    _frames.close();
    _states.close();
    _authFailures.close();
  }
}
