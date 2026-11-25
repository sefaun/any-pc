import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:flutter_webrtc/flutter_webrtc.dart';

import '../input/remote_input.dart';
import '../net/api_client.dart';
import '../net/protocol.dart';
import 'file_transfer.dart';
import 'sfu_session.dart';

enum ViewerState { waiting, connecting, connected, failed, closed }

class RemoteScreen {
  const RemoteScreen(this.index, this.name);
  final int index;
  final String name;
}

/// The controlling side of a session: receives the host's screen through the
/// Cloudflare SFU and sends input, clipboard and files back over data channels.
/// Mirrors desktop/resources/web/js/viewer.js.
class ViewerConnection extends ChangeNotifier implements RemoteInputSink {
  ViewerConnection({
    required this.api,
    required this.connectionId,
    required this.peer,
    required this.sendSignal,
    required this.downloadDirectory,
  });

  final ApiClient api;
  final void Function(Map<String, Object?> payload) sendSignal;
  final String connectionId;
  final DeviceSummary peer;
  final Directory downloadDirectory;
  final RTCVideoRenderer renderer = RTCVideoRenderer();

  ViewerState state = ViewerState.waiting;
  String? error;
  List<RemoteScreen> screens = const [];
  int currentScreen = 0;
  bool inputSupported = true;
  Size frameSize = Size.zero;
  FileTransferService? files;

  /// Emitted when the session fails (the owner ends the connection).
  void Function(String reason)? onFailed;
  void Function(FileTransfer transfer)? onFileReceived;

  RTCPeerConnection? _pc;
  RTCDataChannel? _control;
  bool _started = false;
  bool _closed = false;

  Future<void> start() async {
    await renderer.initialize();
    renderer.onResize = () {
      final size = Size(renderer.videoWidth.toDouble(), renderer.videoHeight.toDouble());
      if (size != frameSize) {
        frameSize = size;
        notifyListeners();
      }
    };
    sendSignal({'kind': 'viewer-ready'});
  }

  void onPeerSignal(Map<String, dynamic> payload) {
    if (payload['kind'] != 'host-ready' || _started || _closed) return;
    _started = true;
    _connect(payload).catchError((Object e) => _fail('$e'));
  }

  Future<void> _connect(Map<String, dynamic> info) async {
    _setState(ViewerState.connecting);
    final trackName = info['trackName'] as String? ?? 'screen';
    final controlName = info['dataChannelName'] as String? ?? 'control';
    final filesName = info['filesChannelName'] as String?;
    inputSupported = info['inputSupported'] != false;

    final ice = await api.get('/rtc/ice-servers');
    final pc = await createPeerConnection({
      'iceServers': ice['iceServers'] ?? const [],
      'bundlePolicy': 'max-bundle',
      'sdpSemantics': 'unified-plan',
    });
    if (_closed) {
      await pc.close();
      return;
    }
    _pc = pc;
    pc.onConnectionState = (s) {
      if (s == RTCPeerConnectionState.RTCPeerConnectionStateFailed) _fail('Media connection failed');
    };
    pc.onTrack = (event) async {
      if (event.track.kind != 'video') return;
      var stream = event.streams.isNotEmpty ? event.streams.first : null;
      if (stream == null) {
        stream = await createLocalMediaStream('remote-screen');
        await stream.addTrack(event.track);
      }
      renderer.srcObject = stream;
      notifyListeners();
    };

    final sfu = SfuSession(api, connectionId, pc);
    await sfu.create();
    await sfu.subscribeTracks([
      {'location': 'remote', 'trackName': trackName, 'kind': 'video'},
    ]);
    await sfu.waitForConnection();
    await sfu.establishDataChannels();
    await sfu.waitForConnection();

    final allocations = await sfu.createDataChannels([
      {'location': 'remote', 'dataChannelName': controlName, 'canReply': true},
      if (filesName != null) {'location': 'remote', 'dataChannelName': filesName, 'canReply': true},
    ]);
    int idOf(String name) => allocations.firstWhere((a) => a['dataChannelName'] == name)['id'] as int;

    if (filesName != null) {
      final channel = await pc.createDataChannel(
        filesName,
        RTCDataChannelInit()
          ..negotiated = true
          ..id = idOf(filesName)
          ..binaryType = 'binary',
      );
      final service = FileTransferService(channel: RtcTransferChannel(channel), downloadDirectory: downloadDirectory)
        ..onFileReceived = (t) => onFileReceived?.call(t);
      service.addListener(notifyListeners);
      channel.onMessage = (m) => service.handleMessage(text: m.isBinary ? null : m.text, binary: m.isBinary ? m.binary : null);
      channel.onDataChannelState = (s) {
        if (s == RTCDataChannelState.RTCDataChannelClosed) service.onChannelClosed();
      };
      files = service;
    }

    final control = await pc.createDataChannel(
      controlName,
      RTCDataChannelInit()
        ..negotiated = true
        ..id = idOf(controlName),
    );
    _control = control;
    control.onMessage = (m) {
      if (!m.isBinary) _onControlMessage(m.text);
    };
    control.onDataChannelState = (s) {
      if (s == RTCDataChannelState.RTCDataChannelOpen) _onControlOpen();
      if (s == RTCDataChannelState.RTCDataChannelClosed && state == ViewerState.connected) _fail('Connection closed');
    };
    if (control.state == RTCDataChannelState.RTCDataChannelOpen) _onControlOpen();
  }

  void _onControlOpen() {
    if (state == ViewerState.connected) return;
    _sendJson({'t': 'hello'});
    _setState(ViewerState.connected);
  }

  void _onControlMessage(String text) {
    Object? decoded;
    try {
      decoded = jsonDecode(text);
    } on FormatException {
      return;
    }
    if (decoded is! Map) return;
    switch (decoded['t']) {
      case 'info':
        final list = decoded['screens'];
        screens = [
          if (list is List)
            for (final s in list)
              if (s is Map && s['index'] is int) RemoteScreen(s['index'] as int, s['name'] is String ? s['name'] as String : ''),
        ];
        currentScreen = decoded['current'] is int ? decoded['current'] as int : 0;
        if (decoded['inputSupported'] is bool) inputSupported = decoded['inputSupported'] as bool;
        notifyListeners();
      case 'clip':
        final clip = decoded['text'];
        if (clip is String && clip.isNotEmpty) Clipboard.setData(ClipboardData(text: clip));
    }
  }

  // ---- Commands -------------------------------------------------------------------------

  void requestScreen(int index) => _sendJson({'t': 'screen', 'index': index});

  Future<bool> sendClipboard() async {
    final data = await Clipboard.getData(Clipboard.kTextPlain);
    final text = data?.text;
    if (text == null || text.isEmpty || text.length > 16 * 1024) return false;
    _sendJson({'t': 'clip', 'text': text});
    return true;
  }

  void sendFile(OutgoingFile file) => files?.send(file);

  // ---- RemoteInputSink ---------------------------------------------------------------------

  @override
  void pointerMove(double x, double y) => _sendJson({'t': 'pm', 'x': x, 'y': y});

  @override
  void pointerButton(int button, bool down, double x, double y) =>
      _sendJson({'t': 'pb', 'b': button, 'd': down, 'x': x, 'y': y});

  @override
  void wheel(double dx, double dy) => _sendJson({'t': 'wh', 'dx': dx.round(), 'dy': dy.round()});

  @override
  void key(String code, bool down) => _sendJson({'t': 'kb', 'c': code, 'd': down});

  @override
  void text(String text) {
    // The host accepts at most 256 characters per event.
    for (var i = 0; i < text.length; i += 200) {
      _sendJson({'t': 'tx', 's': text.substring(i, i + 200 > text.length ? text.length : i + 200)});
    }
  }

  void _sendJson(Map<String, Object?> message) {
    final control = _control;
    if (control == null || control.state != RTCDataChannelState.RTCDataChannelOpen || !inputAllowed(message)) return;
    control.send(RTCDataChannelMessage(jsonEncode(message)));
  }

  bool inputAllowed(Map<String, Object?> message) {
    const inputTypes = {'pm', 'pb', 'wh', 'kb', 'tx'};
    return inputSupported || !inputTypes.contains(message['t']);
  }

  // ---- Lifecycle ---------------------------------------------------------------------------

  void _fail(String reason) {
    if (_closed || state == ViewerState.failed) return;
    error = reason;
    _setState(ViewerState.failed);
    onFailed?.call(reason);
  }

  void _setState(ViewerState next) {
    state = next;
    notifyListeners();
  }

  Future<void> close() async {
    if (_closed) return;
    _closed = true;
    files?.onChannelClosed();
    state = ViewerState.closed;
    final pc = _pc;
    _pc = null;
    _control = null;
    try {
      await pc?.close();
    } catch (_) {}
    renderer.srcObject = null;
    await renderer.dispose();
    notifyListeners();
  }
}
