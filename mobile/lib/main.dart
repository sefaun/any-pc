import 'package:flutter/material.dart';

import 'src/app_controller.dart';
import 'src/core/app_settings.dart';
import 'src/ui/app_scope.dart';
import 'src/ui/home_screen.dart';
import 'src/ui/setup_screen.dart';
import 'src/ui/theme.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  final settings = await AppSettings.load();
  final controller = AppController(settings);
  runApp(AnyPcApp(controller: controller));
  await controller.start();
}

class AnyPcApp extends StatelessWidget {
  const AnyPcApp({super.key, required this.controller});
  final AppController controller;

  @override
  Widget build(BuildContext context) {
    return AppScope(
      controller: controller,
      child: MaterialApp(
        title: 'AnyPC',
        debugShowCheckedModeBanner: false,
        theme: AppTheme.light(),
        darkTheme: AppTheme.dark(),
        home: Builder(
          builder: (context) => AppScope.of(context).serverUrl == null ? const SetupScreen() : const HomeScreen(),
        ),
      ),
    );
  }
}
