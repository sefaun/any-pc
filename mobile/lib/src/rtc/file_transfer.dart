import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter/foundation.dart';
import 'package:flutter_webrtc/flutter_webrtc.dart';

enum TransferDirection { outgoing, incoming }

enum TransferState { queued, transferring, completed, failed }

class FileTransfer {
  FileTransfer({required this.id, required this.direction, required this.name, required this.size, this.path});

  final String id;
  final TransferDirection direction;
  final String name;
  final int size;
  String? path;
  int transferred = 0;
  TransferState state = TransferState.queued;
  String? error;

  double get progress => size <= 0 ? (state == TransferState.completed ? 1 : 0) : transferred / size;
}

/// Source of an outgoing file (decoupled from the file picker for testing).
class OutgoingFile {
  OutgoingFile({required this.name, required this.size, required this.open});
  final String name;
  final int size;
  final Stream<List<int>> Function() open;
}

/// Abstraction over a data channel so the protocol can be unit tested.
abstract class TransferChannel {
  bool get isOpen;
  int get bufferedAmount;
  Future<void> sendText(String text);
  Future<void> sendBinary(Uint8List bytes);
}

class RtcTransferChannel implements TransferChannel {
  RtcTransferChannel(this.channel);
  final RTCDataChannel channel;
  @override
  bool get isOpen => channel.state == RTCDataChannelState.RTCDataChannelOpen;
  @override
  int get bufferedAmount => channel.bufferedAmount ?? 0;
  @override
  Future<void> sendText(String text) => channel.send(RTCDataChannelMessage(text));
  @override
  Future<void> sendBinary(Uint8List bytes) => channel.send(RTCDataChannelMessage.fromBinary(bytes));
}

/// File transfer over the "files" data channel, wire compatible with
/// desktop/resources/web/js/file-transfer.js:
///
///   {t:'file-offer', id, name, size} → binary chunks (≤16 KiB) → {t:'file-end', id}
///   ← {t:'file-ack', id, ok, error}
class FileTransferService extends ChangeNotifier {
  FileTransferService({required this.channel, required this.downloadDirectory});

  static const chunkSize = 16 * 1024;
  static const _highWaterMark = 1024 * 1024;
  static const _ackTimeout = Duration(minutes: 2);

  final TransferChannel channel;
  final Directory downloadDirectory;
  final List<FileTransfer> transfers = [];
  final Map<String, Completer<Map<String, dynamic>>> _acks = {};
  Future<void> _sendQueue = Future.value();
  _Incoming? _incoming;
  int _idCounter = 0;

  /// Called when an incoming file has been saved.
  void Function(FileTransfer transfer)? onFileReceived;

  // ---- Outgoing ----------------------------------------------------------------------

  void send(OutgoingFile file) {
    final transfer = FileTransfer(
      id: '${DateTime.now().microsecondsSinceEpoch}-${_idCounter++}',
      direction: TransferDirection.outgoing,
      name: file.name,
      size: file.size,
    );
    transfers.add(transfer);
    notifyListeners();
    _sendQueue = _sendQueue.then((_) => _send(transfer, file));
  }

  Future<void> _send(FileTransfer transfer, OutgoingFile file) async {
    try {
      if (!channel.isOpen) throw StateError('Connection closed');
      transfer.state = TransferState.transferring;
      notifyListeners();
      await channel.sendText(jsonEncode({'t': 'file-offer', 'id': transfer.id, 'name': transfer.name, 'size': transfer.size}));
      final pending = BytesBuilder(copy: false);
      await for (final data in file.open()) {
        pending.add(data);
        while (pending.length >= chunkSize) {
          final bytes = pending.takeBytes();
          await _sendChunk(transfer, Uint8List.sublistView(bytes, 0, chunkSize));
          if (bytes.length > chunkSize) pending.add(Uint8List.sublistView(bytes, chunkSize));
        }
      }
      if (pending.isNotEmpty) await _sendChunk(transfer, pending.takeBytes());
      final ack = _waitForAck(transfer.id);
      await channel.sendText(jsonEncode({'t': 'file-end', 'id': transfer.id}));
      final result = await ack;
      if (result['ok'] == true && transfer.transferred == transfer.size) {
        transfer.state = TransferState.completed;
      } else {
        _fail(transfer, result['error'] is String && (result['error'] as String).isNotEmpty ? result['error'] as String : 'Transfer failed');
      }
    } catch (e) {
      _fail(transfer, '$e');
    }
    notifyListeners();
  }

  Future<void> _sendChunk(FileTransfer transfer, Uint8List bytes) async {
    while (channel.bufferedAmount > _highWaterMark) {
      if (!channel.isOpen) throw StateError('Connection closed');
      await Future<void>.delayed(const Duration(milliseconds: 20));
    }
    if (!channel.isOpen) throw StateError('Connection closed');
    await channel.sendBinary(bytes);
    transfer.transferred += bytes.length;
    notifyListeners();
  }

  Future<Map<String, dynamic>> _waitForAck(String id) {
    final completer = Completer<Map<String, dynamic>>();
    _acks[id] = completer;
    return completer.future.timeout(_ackTimeout, onTimeout: () {
      _acks.remove(id);
      return {'ok': false, 'error': 'No confirmation from the remote side'};
    });
  }

  // ---- Incoming ----------------------------------------------------------------------

  /// Feeds a message received on the files channel.
  Future<void> handleMessage({String? text, Uint8List? binary}) async {
    if (binary != null) {
      final incoming = _incoming;
      if (incoming == null) return;
      incoming.chain = incoming.chain.then((_) => _write(incoming, binary));
      return;
    }
    if (text == null) return;
    Object? decoded;
    try {
      decoded = jsonDecode(text);
    } on FormatException {
      return;
    }
    if (decoded is! Map<String, dynamic>) return;
    switch (decoded['t']) {
      case 'file-offer':
        _onOffer(decoded);
      case 'file-end':
        await _onEnd(decoded['id']);
      case 'file-ack':
        _acks.remove(decoded['id'])?.complete(decoded);
    }
  }

  void _onOffer(Map<String, dynamic> message) {
    final id = message['id'];
    final name = message['name'];
    final size = message['size'];
    if (id is! String || name is! String || size is! num || size < 0) return;
    final previous = _incoming;
    if (previous != null) {
      _fail(previous.transfer, 'Interrupted by a new transfer');
      unawaited(previous.close(delete: true));
    }
    final transfer = FileTransfer(id: id, direction: TransferDirection.incoming, name: sanitizeFileName(name), size: size.toInt());
    transfers.add(transfer);
    final incoming = _Incoming(transfer);
    _incoming = incoming;
    incoming.chain = _open(incoming);
    notifyListeners();
  }

  Future<void> _open(_Incoming incoming) async {
    try {
      await downloadDirectory.create(recursive: true);
      final path = await uniquePath(downloadDirectory, incoming.transfer.name);
      incoming.transfer.path = path;
      incoming.part = File('$path.anypc-part');
      incoming.sink = incoming.part!.openWrite();
      incoming.transfer.state = TransferState.transferring;
    } catch (e) {
      _fail(incoming.transfer, 'Cannot save the file: $e');
    }
  }

  Future<void> _write(_Incoming incoming, Uint8List data) async {
    final sink = incoming.sink;
    final t = incoming.transfer;
    if (sink == null || t.state != TransferState.transferring) return;
    if (t.transferred + data.length > t.size) {
      _fail(t, 'Received more data than announced');
      await incoming.close(delete: true);
      return;
    }
    sink.add(data);
    t.transferred += data.length;
    notifyListeners();
  }

  Future<void> _onEnd(Object? id) async {
    final incoming = _incoming;
    if (incoming == null || incoming.transfer.id != id) return;
    _incoming = null;
    await incoming.chain;
    final t = incoming.transfer;
    var ok = false;
    var error = '';
    if (t.state != TransferState.transferring) {
      error = t.error ?? 'The remote side could not save the file';
    } else if (t.transferred != t.size) {
      _fail(t, 'Incomplete file');
      error = 'The file could not be saved completely';
      await incoming.close(delete: true);
    } else {
      await incoming.close(delete: false);
      try {
        await incoming.part!.rename(t.path!);
        t.state = TransferState.completed;
        ok = true;
        onFileReceived?.call(t);
      } catch (e) {
        _fail(t, 'Cannot save the file: $e');
        error = 'The file could not be saved';
      }
    }
    notifyListeners();
    try {
      await channel.sendText(jsonEncode({'t': 'file-ack', 'id': t.id, 'ok': ok, 'error': error}));
    } catch (_) {
      // Channel closed; the sender times out.
    }
  }

  // ---- Common ------------------------------------------------------------------------

  /// The channel closed: fail everything in flight.
  void onChannelClosed() {
    final incoming = _incoming;
    _incoming = null;
    if (incoming != null) {
      _fail(incoming.transfer, 'Connection closed');
      unawaited(incoming.close(delete: true));
    }
    for (final completer in _acks.values) {
      completer.complete({'ok': false, 'error': 'Connection closed'});
    }
    _acks.clear();
    for (final t in transfers) {
      if (t.state == TransferState.queued || t.state == TransferState.transferring) _fail(t, 'Connection closed');
    }
    notifyListeners();
  }

  void _fail(FileTransfer transfer, String error) {
    if (transfer.state == TransferState.completed || transfer.state == TransferState.failed) return;
    transfer.state = TransferState.failed;
    transfer.error = error;
  }

  /// Keeps only the last path component and strips characters invalid on common file systems.
  static String sanitizeFileName(String name) {
    var base = name.replaceAll('\\', '/').split('/').last;
    base = base.replaceAll(RegExp(r'[<>:"|?*\x00-\x1f]'), '_').trim();
    base = base.replaceAll(RegExp(r'^\.+'), '').replaceAll(RegExp(r'[. ]+$'), '');
    if (base.isEmpty) return 'file';
    if (base.length > 200) {
      final dot = base.lastIndexOf('.');
      final ext = dot > 0 && base.length - dot <= 17 ? base.substring(dot) : '';
      base = base.substring(0, 200 - ext.length) + ext;
    }
    return base;
  }

  /// A path in [directory] that does not exist yet ("a (1).txt", ...).
  static Future<String> uniquePath(Directory directory, String name) async {
    final dot = name.lastIndexOf('.');
    final stem = dot > 0 ? name.substring(0, dot) : name;
    final ext = dot > 0 ? name.substring(dot) : '';
    var candidate = '${directory.path}/$name';
    var i = 1;
    while (await File(candidate).exists() || await File('$candidate.anypc-part').exists()) {
      candidate = '${directory.path}/$stem ($i)$ext';
      i++;
    }
    return candidate;
  }
}

class _Incoming {
  _Incoming(this.transfer);
  final FileTransfer transfer;
  File? part;
  IOSink? sink;
  Future<void> chain = Future.value();

  Future<void> close({required bool delete}) async {
    final s = sink;
    sink = null;
    if (s != null) {
      await s.flush();
      await s.close();
    }
    if (delete) {
      try {
        await part?.delete();
      } catch (_) {}
    }
  }
}
