import 'dart:async';
import 'dart:io';
import 'dart:math';

import 'package:flutter/foundation.dart';
import 'package:path_provider/path_provider.dart';

import 'core/app_settings.dart';
import 'core/recent_connections.dart';
import 'core/server_url.dart';
import 'net/api_client.dart';
import 'net/protocol.dart';
import 'net/signaling_client.dart';
import 'rtc/file_transfer.dart';
import 'rtc/viewer_connection.dart';

const String appVersion = String.fromEnvironment('APP_VERSION', defaultValue: '0.1.0');

enum OutgoingState { idle, requesting, waiting }

/// Why a session ended, for the UI.
class SessionEnd {
  const SessionEnd(this.peer, this.reason, {required this.byUser});
  final DeviceSummary peer;
  final String reason;
  final bool byUser;
}

/// Application state: device registration, signaling, outgoing requests and
/// the active viewer session.
class AppController extends ChangeNotifier {
  AppController(this.settings)
      : api = ApiClient(userAgent: 'AnyPC-Mobile/$appVersion'),
        signaling = SignalingClient() {
    recent = settings.recentConnections;
    _subscriptions.addAll([
      signaling.frames.listen(_onFrame),
      signaling.states.listen((_) => _onSignalingState()),
      signaling.authenticationFailures.listen((_) => authenticate()),
    ]);
  }

  final AppSettings settings;
  final ApiClient api;
  final SignalingClient signaling;
  final List<StreamSubscription<dynamic>> _subscriptions = [];
  final _messages = StreamController<String>.broadcast();
  final _sessionStarts = StreamController<ViewerConnection>.broadcast();
  final _sessionEnds = StreamController<SessionEnd>.broadcast();

  late RecentConnectionList recent;
  String? deviceId;
  String? lastError;
  OutgoingState outgoing = OutgoingState.idle;
  DeviceSummary? outgoingTarget;
  ViewerConnection? session;

  String? _requestId;
  String? _outgoingConnectionId;
  Timer? _refreshTimer;
  Timer? _retryTimer;
  int _retryAttempt = 0;
  bool _authInFlight = false;

  Stream<String> get messages => _messages.stream;
  Stream<ViewerConnection> get sessionStarts => _sessionStarts.stream;
  Stream<SessionEnd> get sessionEnds => _sessionEnds.stream;
  Uri? get serverUrl => settings.serverUrl;
  bool get isOnline => signaling.isConnected;
  SignalingState get signalingState => signaling.state;

  static String get platformId => Platform.isIOS ? 'ios' : (Platform.isAndroid ? 'android' : 'unknown');

  Future<void> start() async {
    if (serverUrl != null) await authenticate();
  }

  Future<void> setServer(Uri url) async {
    await endSession(byUser: true);
    signaling.disconnect();
    await settings.setServerUrl(url);
    api.accessToken = null;
    _retryAttempt = 0;
    notifyListeners();
    await authenticate();
  }

  Future<void> setDeviceName(String name) async {
    await settings.setDeviceName(name);
    await authenticate();
  }

  // ---- Authentication ------------------------------------------------------------------

  Future<void> authenticate({String? platformOverride}) async {
    final base = serverUrl;
    if (base == null || _authInFlight) return;
    _authInFlight = true;
    _retryTimer?.cancel();
    api.baseUrl = base;
    api.accessToken = null;
    final body = <String, Object?>{
      'name': settings.deviceName,
      'platform': platformOverride ?? platformId,
      'appVersion': appVersion,
      if (settings.deviceId != null && settings.deviceSecret != null) ...{
        'deviceId': settings.deviceId,
        'deviceSecret': settings.deviceSecret,
      },
    };
    try {
      final response = await api.post('/devices/register', body);
      _authInFlight = false;
      final id = response['deviceId'] as String;
      final secret = response['deviceSecret'];
      if (secret is String) await settings.setDeviceCredentials(id, secret);
      deviceId = id;
      lastError = null;
      _retryAttempt = 0;
      final token = response['accessToken'] as String;
      api.accessToken = token;
      if (signaling.state == SignalingState.disconnected) {
        signaling.connect(base, token);
      } else {
        signaling.updateToken(token);
      }
      final expiresIn = response['expiresIn'] is int ? response['expiresIn'] as int : 3600;
      _refreshTimer?.cancel();
      _refreshTimer = Timer(Duration(seconds: max(60, expiresIn * 8 ~/ 10)), authenticate);
      notifyListeners();
    } on ApiException catch (e) {
      _authInFlight = false;
      if (e.status == 400 && platformOverride == null && e.code == 'VALIDATION_FAILED') {
        // Servers before 0.4 only know desktop platforms.
        return authenticate(platformOverride: 'unknown');
      }
      if (e.status == 401) {
        await settings.clearDeviceCredentials();
        return authenticate(platformOverride: platformOverride);
      }
      lastError = e.message;
      notifyListeners();
      final delay = min(1000 * (1 << min(_retryAttempt++, 5)), 30000);
      _retryTimer = Timer(Duration(milliseconds: delay), authenticate);
    }
  }

  // ---- Outgoing connections ------------------------------------------------------------

  void connectTo(String id, {String password = ''}) {
    if (!signaling.isConnected) {
      _messages.add('Not connected to the AnyPC server.');
      return;
    }
    if (outgoing != OutgoingState.idle || session != null) {
      _messages.add('A connection is already in progress.');
      return;
    }
    if (id == deviceId) {
      _messages.add('You cannot connect to this device itself.');
      return;
    }
    _requestId = '${DateTime.now().microsecondsSinceEpoch}';
    outgoing = OutgoingState.requesting;
    outgoingTarget = DeviceSummary(id: id, name: recent.find(id)?.displayName ?? '', platform: '');
    _updateRecent(recent.recordAttempt(id));
    signaling.send(ClientEvent.connectionRequest, {
      'targetId': id,
      'requestId': _requestId,
      if (password.isNotEmpty) 'password': password,
    });
    notifyListeners();
  }

  void cancelOutgoing() {
    final id = _outgoingConnectionId;
    if (id != null) signaling.send(ClientEvent.connectionEnd, {'connectionId': id});
    _clearOutgoing();
  }

  void _clearOutgoing() {
    _requestId = null;
    _outgoingConnectionId = null;
    outgoing = OutgoingState.idle;
    outgoingTarget = null;
    notifyListeners();
  }

  // ---- Recent connections --------------------------------------------------------------

  void renameRecent(String id, String alias) => _updateRecent(recent.rename(id, alias));
  void removeRecent(String id) => _updateRecent(recent.remove(id));

  void _updateRecent(RecentConnectionList list) {
    recent = list;
    unawaited(settings.setRecentConnections(list));
    notifyListeners();
  }

  // ---- Session -------------------------------------------------------------------------

  Future<void> endSession({bool byUser = true, String? reason}) async {
    final current = session;
    if (current == null) return;
    // Cleared first so the server's echo of connection.ended is ignored.
    session = null;
    signaling.send(ClientEvent.connectionEnd, {'connectionId': current.connectionId});
    await current.close();
    _sessionEnds.add(SessionEnd(current.peer, reason ?? (byUser ? 'You ended the session.' : 'Session ended.'), byUser: byUser));
    notifyListeners();
  }

  Future<void> _startSession(String connectionId, DeviceSummary peer) async {
    final directory = await _downloadDirectory();
    final connection = ViewerConnection(
      api: api,
      connectionId: connectionId,
      peer: peer,
      downloadDirectory: directory,
      sendSignal: (payload) => signaling.send(ClientEvent.connectionSignal, {'connectionId': connectionId, 'payload': payload}),
    );
    connection.onFailed = (reason) => endSession(byUser: false, reason: reason);
    connection.onFileReceived = (t) => _messages.add('Received ${t.name}');
    session = connection;
    notifyListeners();
    await connection.start();
    _sessionStarts.add(connection);
  }

  static Future<Directory> _downloadDirectory() async {
    final base = await getApplicationDocumentsDirectory();
    return Directory('${base.path}/AnyPC');
  }

  // ---- Signaling -----------------------------------------------------------------------

  void _onSignalingState() {
    if (!signaling.isConnected) {
      if (outgoing != OutgoingState.idle) _clearOutgoing();
      if (session != null) unawaited(endSession(byUser: false, reason: 'Lost connection to the AnyPC server.'));
    }
    notifyListeners();
  }

  void _onFrame(ServerFrame frame) {
    switch (frame.event) {
      case ServerEvent.connectionPending:
        if (frame.str('requestId') != _requestId) return;
        _outgoingConnectionId = frame.str('connectionId');
        final target = DeviceSummary.fromJson(frame.data['target']);
        outgoingTarget = target;
        outgoing = OutgoingState.waiting;
        _updateRecent(recent.updateDetails(target.id, name: target.name, platform: target.platform));
      case ServerEvent.connectionAccepted:
        if (frame.str('role') != 'viewer' || frame.str('connectionId') != _outgoingConnectionId) return;
        final peer = DeviceSummary.fromJson(frame.data['peer']);
        _updateRecent(recent.updateDetails(peer.id, name: peer.name, platform: peer.platform));
        final connectionId = frame.str('connectionId');
        _clearOutgoing();
        unawaited(_startSession(connectionId, peer));
      case ServerEvent.connectionRejected:
        if (frame.str('connectionId') != _outgoingConnectionId) return;
        _clearOutgoing();
        _messages.add(frame.str('reason') == 'timeout' ? 'The remote side did not answer.' : 'The remote side declined the connection.');
      case ServerEvent.connectionEnded:
        final id = frame.str('connectionId');
        if (id == _outgoingConnectionId) {
          _clearOutgoing();
          _messages.add('The connection request ended.');
        }
        if (session?.connectionId == id) {
          unawaited(endSession(byUser: false, reason: _describeEnd(frame.str('reason'))));
        }
      case ServerEvent.connectionSignal:
        if (session?.connectionId == frame.str('connectionId')) {
          final payload = frame.data['payload'];
          if (payload is Map<String, dynamic>) session!.onPeerSignal(payload);
        }
      case ServerEvent.connectionIncoming:
        // This app can control computers but cannot be controlled itself.
        signaling.send(ClientEvent.connectionRespond, {'connectionId': frame.str('connectionId'), 'accept': false});
        final from = DeviceSummary.fromJson(frame.data['from']);
        _messages.add('Declined a request from ${from.name}: phones cannot be controlled remotely yet.');
      case ServerEvent.error:
        if (frame.str('requestId').isNotEmpty && frame.str('requestId') == _requestId) {
          _clearOutgoing();
          _messages.add(switch (frame.str('code')) {
            'DEVICE_NOT_FOUND' => 'No device with this address exists.',
            'DEVICE_OFFLINE' => 'The remote device is offline.',
            _ => frame.str('message'),
          });
        }
    }
    notifyListeners();
  }

  static String _describeEnd(String reason) => switch (reason) {
        'hangup' => 'The remote side ended the session.',
        'peer_disconnected' => 'The remote device disconnected.',
        _ => 'Session ended.',
      };

  /// Checks that [url] hosts an AnyPC backend.
  static Future<void> verifyServer(Uri url) async {
    final client = ApiClient(userAgent: 'AnyPC-Mobile/$appVersion')..baseUrl = url;
    try {
      final health = await client.get('/health');
      if (health['status'] != 'ok') throw ApiException(0, 'The address did not respond like an AnyPC server');
    } finally {
      client.close();
    }
  }

  static Uri? parseServer(String input) => normalizeServerUrl(input);

  static List<FileTransfer> transfersOf(ViewerConnection? c) => c?.files?.transfers ?? const [];

  @override
  void dispose() {
    for (final s in _subscriptions) {
      s.cancel();
    }
    _refreshTimer?.cancel();
    _retryTimer?.cancel();
    signaling.dispose();
    api.close();
    _messages.close();
    _sessionStarts.close();
    _sessionEnds.close();
    super.dispose();
  }
}
