/// Signaling vocabulary shared with backend/src/signaling/signaling.protocol.ts.
abstract final class ClientEvent {
  static const connectionRequest = 'connection.request';
  static const connectionRespond = 'connection.respond';
  static const connectionEnd = 'connection.end';
  static const connectionSignal = 'connection.signal';
}

abstract final class ServerEvent {
  static const sessionReady = 'session.ready';
  static const connectionPending = 'connection.pending';
  static const connectionIncoming = 'connection.incoming';
  static const connectionAccepted = 'connection.accepted';
  static const connectionRejected = 'connection.rejected';
  static const connectionEnded = 'connection.ended';
  static const connectionSignal = 'connection.signal';
  static const error = 'error';
}

/// WebSocket close codes used by the server.
abstract final class CloseCode {
  static const unauthorized = 4401;
  static const replaced = 4001;
}

class DeviceSummary {
  const DeviceSummary({required this.id, required this.name, required this.platform});

  final String id;
  final String name;
  final String platform;

  factory DeviceSummary.fromJson(Object? json) {
    final map = json is Map ? json : const {};
    return DeviceSummary(
      id: map['id'] is String ? map['id'] as String : '',
      name: map['name'] is String ? map['name'] as String : '',
      platform: map['platform'] is String ? map['platform'] as String : '',
    );
  }
}

/// One frame received from the signaling server.
class ServerFrame {
  const ServerFrame(this.event, this.data);

  final String event;
  final Map<String, dynamic> data;

  String str(String key) => data[key] is String ? data[key] as String : '';
  int integer(String key) => data[key] is num ? (data[key] as num).toInt() : 0;
}
