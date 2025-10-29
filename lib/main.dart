import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'services/api_service.dart';
import 'screens/register_screen.dart';
import 'screens/chat_screen.dart';
import 'screens/devices_screen.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:hive_flutter/hive_flutter.dart';
import 'services/storage_service.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Hive.initFlutter();
  await StorageService.init();
  runApp(const ChatridgeApp());
}

class ChatridgeApp extends StatelessWidget {
  const ChatridgeApp({super.key});

  Future<bool> _hasRegistration() async {
    final sp = await SharedPreferences.getInstance();
    return sp.containsKey('username') && sp.containsKey('deviceName');
  }

  @override
  Widget build(BuildContext context) {
    return ChangeNotifierProvider(
      create: (_) => ApiService(),
      child: MaterialApp(
        title: 'Chatridge',
        theme: ThemeData(primarySwatch: Colors.teal),
        home: FutureBuilder<bool>(
          future: _hasRegistration(),
          builder: (context, snap) {
            if (!snap.hasData) {
              return const Scaffold(
                  body: Center(child: CircularProgressIndicator()));
            }
            return snap.data! ? const ChatScreen() : const RegisterScreen();
          },
        ),
        routes: {
          '/chat': (_) => const ChatScreen(),
          '/register': (_) => const RegisterScreen(),
          '/devices': (_) => const DevicesScreen(),
        },
      ),
    );
  }
}
