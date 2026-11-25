import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:math';
import 'dart:typed_data';

import 'package:anypc_mobile/src/rtc/file_transfer.dart';
import 'package:flutter_test/flutter_test.dart';

/// Two in-memory channels wired back to back, like a data channel through the SFU.
class LoopbackChannel implements TransferChannel {
  late FileTransferService peer;
  bool open = true;
  int sentBinary = 0;

  @override
  bool get isOpen => open;
  @override
  int get bufferedAmount => 0;
  @override
  Future<void> sendText(String text) async => scheduleMicrotask(() => peer.handleMessage(text: text));
  @override
  Future<void> sendBinary(Uint8List bytes) async {
    sentBinary++;
    final copy = Uint8List.fromList(bytes);
    scheduleMicrotask(() => peer.handleMessage(binary: copy));
  }
}

Future<void> settle(bool Function() done) async {
  for (var i = 0; i < 500 && !done(); i++) {
    await Future<void>.delayed(const Duration(milliseconds: 5));
  }
}

void main() {
  late Directory temp;

  setUp(() async => temp = await Directory.systemTemp.createTemp('anypc_test'));
  tearDown(() async => temp.delete(recursive: true));

  test('sanitizes file names', () {
    expect(FileTransferService.sanitizeFileName('report.pdf'), 'report.pdf');
    expect(FileTransferService.sanitizeFileName('../../etc/passwd'), 'passwd');
    expect(FileTransferService.sanitizeFileName(r'C:\Users\x\notes.txt'), 'notes.txt');
    expect(FileTransferService.sanitizeFileName('.bashrc'), 'bashrc');
    expect(FileTransferService.sanitizeFileName('a<b>:c?.txt'), 'a_b__c_.txt');
    expect(FileTransferService.sanitizeFileName('...'), 'file');
  });

  test('unique paths avoid overwriting', () async {
    await File('${temp.path}/a.txt').writeAsString('x');
    expect(await FileTransferService.uniquePath(temp, 'a.txt'), '${temp.path}/a (1).txt');
    expect(await FileTransferService.uniquePath(temp, 'b.txt'), '${temp.path}/b.txt');
  });

  test('loopback transfer preserves content in both directions', () async {
    final aChannel = LoopbackChannel();
    final bChannel = LoopbackChannel();
    final a = FileTransferService(channel: aChannel, downloadDirectory: Directory('${temp.path}/a'));
    final b = FileTransferService(channel: bChannel, downloadDirectory: Directory('${temp.path}/b'));
    aChannel.peer = b;
    bChannel.peer = a;

    final random = Random(42);
    final content = Uint8List.fromList(List.generate(FileTransferService.chunkSize * 3 + 123, (_) => random.nextInt(256)));
    a.send(OutgoingFile(
      name: 'data.bin',
      size: content.length,
      // Odd-sized pieces exercise the re-chunking logic.
      open: () => Stream.fromIterable([for (var i = 0; i < content.length; i += 7000) content.sublist(i, min(i + 7000, content.length))]),
    ));
    b.send(OutgoingFile(name: 'hello.txt', size: 5, open: () => Stream.value(utf8.encode('hello'))));

    await settle(() => a.transfers.every((t) => t.state == TransferState.completed || t.state == TransferState.failed) &&
        b.transfers.length == 2 && a.transfers.length == 2);
    await settle(() => [...a.transfers, ...b.transfers].every((t) => t.state == TransferState.completed));

    expect(a.transfers.map((t) => t.state), everyElement(TransferState.completed));
    expect(b.transfers.map((t) => t.state), everyElement(TransferState.completed));
    expect(await File('${temp.path}/b/data.bin').readAsBytes(), content);
    expect(await File('${temp.path}/a/hello.txt').readAsString(), 'hello');
    expect(aChannel.sentBinary, 4);
  });

  test('incomplete files are discarded and reported', () async {
    final channel = LoopbackChannel();
    final sender = FileTransferService(channel: LoopbackChannel(), downloadDirectory: temp);
    final receiver = FileTransferService(channel: channel, downloadDirectory: Directory('${temp.path}/in'));
    channel.peer = sender;
    await receiver.handleMessage(text: jsonEncode({'t': 'file-offer', 'id': 'x', 'name': 'big.bin', 'size': 100}));
    await receiver.handleMessage(binary: Uint8List(10));
    await receiver.handleMessage(text: jsonEncode({'t': 'file-end', 'id': 'x'}));
    expect(receiver.transfers.single.state, TransferState.failed);
    expect(Directory('${temp.path}/in').listSync(), isEmpty);
  });

  test('closing the channel fails transfers in flight', () async {
    final channel = LoopbackChannel()..open = false;
    final service = FileTransferService(channel: channel, downloadDirectory: temp);
    service.send(OutgoingFile(name: 'f.txt', size: 1, open: () => Stream.value([1])));
    await settle(() => service.transfers.single.state == TransferState.failed);
    expect(service.transfers.single.state, TransferState.failed);
  });
}
