import 'package:anypc_mobile/main.dart';
import 'package:anypc_mobile/src/app_controller.dart';
import 'package:anypc_mobile/src/core/app_settings.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';

void main() {
  testWidgets('first launch asks for the server and validates the address', (tester) async {
    SharedPreferences.setMockInitialValues({});
    final controller = AppController(await AppSettings.load());
    await tester.pumpWidget(AnyPcApp(controller: controller));

    expect(find.text('Connect to your AnyPC server'), findsOneWidget);
    await tester.enterText(find.byType(TextField), 'ftp://nope');
    await tester.tap(find.text('Connect'));
    await tester.pump();
    expect(find.textContaining('valid address'), findsOneWidget);
  });

  testWidgets('home lists recent sessions with their names', (tester) async {
    SharedPreferences.setMockInitialValues({
      'serverUrl': 'https://anypc.example.com',
      'recentConnections':
          '[{"id":"132532630","name":"Office PC","platform":"windows","alias":"","lastConnected":null,"count":3}]',
    });
    final controller = AppController(await AppSettings.load());
    await tester.pumpWidget(AnyPcApp(controller: controller));
    await tester.pump();

    expect(find.text('Remote computer'), findsOneWidget);
    expect(find.text('Office PC'), findsOneWidget);
    expect(find.textContaining('132 532 630'), findsOneWidget);

    // Connecting while offline is refused with a message rather than crashing.
    await tester.tap(find.text('Office PC'));
    await tester.pump();
    expect(find.text('Not connected to the AnyPC server.'), findsOneWidget);
    expect(find.byType(SnackBar), findsOneWidget);
  });
}
