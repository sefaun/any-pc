import 'dart:io';

import 'package:anypc_mobile/src/net/api_client.dart';
import 'package:anypc_mobile/src/net/protocol.dart';
import 'package:anypc_mobile/src/rtc/viewer_connection.dart';
import 'package:anypc_mobile/src/ui/viewer_screen.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('the session screen closes when the connection ends', (tester) async {
    final connection = ViewerConnection(
      api: ApiClient(),
      connectionId: 'c1',
      peer: const DeviceSummary(id: '123456789', name: 'Office PC', platform: 'windows'),
      sendSignal: (_) {},
      downloadDirectory: Directory.systemTemp,
    );
    await tester.pumpWidget(MaterialApp(
      home: Builder(
        builder: (context) => TextButton(
          onPressed: () => Navigator.of(context).push(MaterialPageRoute<void>(builder: (_) => ViewerScreen(connection: connection))),
          child: const Text('open'),
        ),
      ),
    ));
    await tester.tap(find.text('open'));
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 500));
    expect(find.byType(ViewerScreen), findsOneWidget);

    // A dialog on top of the session screen must not keep it open.
    await tester.tap(find.byTooltip('Disconnect'));
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 500));
    expect(find.text('Disconnect?'), findsOneWidget);

    await tester.runAsync(connection.close);
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 500));
    expect(find.byType(ViewerScreen), findsNothing);
    expect(find.text('Disconnect?'), findsNothing);
    expect(find.text('open'), findsOneWidget);
  });

  test('the host controls the sound and its messages are understood', () {
    final connection = ViewerConnection(
      api: ApiClient(),
      connectionId: 'c2',
      peer: const DeviceSummary(id: '123456789', name: 'Office PC', platform: 'windows'),
      sendSignal: (_) {},
      downloadDirectory: Directory.systemTemp,
    );
    expect(connection.audioEnabled, isTrue);
    connection.handleControlMessage('{"t":"audio","available":true,"enabled":false}');
    expect(connection.audioEnabled, isFalse);
    connection.handleControlMessage('{"t":"info","screens":[],"current":0,"audio":{"available":true,"enabled":true}}');
    expect(connection.audioEnabled, isTrue);
    connection.handleControlMessage('not json');
    expect(connection.audioEnabled, isTrue);
  });
}
