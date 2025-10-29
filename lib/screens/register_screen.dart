import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/api_service.dart';
import 'package:shared_preferences/shared_preferences.dart';
import '../services/permission_service.dart';

class RegisterScreen extends StatefulWidget {
  const RegisterScreen({super.key});

  @override
  State<RegisterScreen> createState() => _RegisterScreenState();
}

class _RegisterScreenState extends State<RegisterScreen> {
  final _formKey = GlobalKey<FormState>();
  final _usernameCtrl = TextEditingController();
  final _deviceCtrl = TextEditingController();
  bool _loading = false;

  @override
  void dispose() {
    _usernameCtrl.dispose();
    _deviceCtrl.dispose();
    super.dispose();
  }

  Future<void> _register() async {
    if (!_formKey.currentState!.validate()) return;
    // Request storage permission before registering (location is optional).
    // On some Android versions location permission is required to read SSID;
    // we'll ask for it later only when needed.
    final okPerm = await PermissionService.requestStoragePermission();
    if (!okPerm) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('Storage permission not granted')));
      }
      return;
    }
    setState(() => _loading = true);
    final api = Provider.of<ApiService>(context, listen: false);
    final username = _usernameCtrl.text.trim();
    final deviceName = _deviceCtrl.text.trim();

    try {
      final ok = await api.register(username: username, deviceName: deviceName);
      if (ok) {
        final sp = await SharedPreferences.getInstance();
        await sp.setString('username', username);
        await sp.setString('deviceName', deviceName);
        if (!mounted) return;
        Navigator.of(context).pushReplacementNamed('/chat');
      } else {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
              const SnackBar(content: Text('Registration failed')));
        }
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context)
            .showSnackBar(SnackBar(content: Text('Error: $e')));
      }
    } finally {
      if (mounted) setState(() => _loading = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Register Device')),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Form(
          key: _formKey,
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              TextFormField(
                controller: _usernameCtrl,
                decoration: const InputDecoration(labelText: 'Username'),
                validator: (v) =>
                    (v == null || v.trim().isEmpty) ? 'Enter username' : null,
              ),
              TextFormField(
                controller: _deviceCtrl,
                decoration: const InputDecoration(labelText: 'Device Name'),
                validator: (v) => (v == null || v.trim().isEmpty)
                    ? 'Enter device name'
                    : null,
              ),
              const SizedBox(height: 16),
              ElevatedButton.icon(
                onPressed: _loading ? null : _register,
                icon: _loading
                    ? const SizedBox(
                        width: 16,
                        height: 16,
                        child: CircularProgressIndicator(strokeWidth: 2))
                    : const Icon(Icons.check),
                label: const Text('Register & Start Chatting'),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
