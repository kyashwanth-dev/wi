import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/api_service.dart';
import '../models/device.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:file_picker/file_picker.dart';
import 'package:dio/dio.dart';
import 'device_chat_screen.dart';
import '../services/permission_service.dart';

class DevicesScreen extends StatefulWidget {
  const DevicesScreen({super.key});

  @override
  State<DevicesScreen> createState() => _DevicesScreenState();
}

class _DevicesScreenState extends State<DevicesScreen> {
  List<Device> _devices = [];
  bool _loading = false;
  String? _username;

  @override
  void initState() {
    super.initState();
    _loadUsername();
    _fetch();
  }

  Future<void> _loadUsername() async {
    final sp = await SharedPreferences.getInstance();
    if (!mounted) return;
    setState(() => _username = sp.getString('username'));
  }

  Future<void> _fetch() async {
    setState(() => _loading = true);
    try {
      final api = Provider.of<ApiService>(context, listen: false);
      final devices = await api.getDevices();
      if (!mounted) return;
      setState(() => _devices = devices);
    } catch (e) {
      // ignore
    } finally {
      if (mounted) setState(() => _loading = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Connected Devices')),
      body: RefreshIndicator(
        onRefresh: _fetch,
        child: _loading
            ? const Center(child: CircularProgressIndicator())
            : ListView.builder(
                itemCount: _devices.length,
                itemBuilder: (context, i) {
                  final d = _devices[i];
                  return ListTile(
                    leading: const Icon(Icons.smartphone),
                    title: Text(d.deviceName),
                    subtitle: Text('${d.username} — ${d.ip}'),
                    onTap: () => _showDeviceActions(d),
                  );
                },
              ),
      ),
    );
  }

  void _showDeviceActions(Device device) {
    showModalBottomSheet(
      context: context,
      builder: (_) => SafeArea(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            ListTile(
              leading: const Icon(Icons.chat),
              title: const Text('Open chat'),
              onTap: () {
                Navigator.of(context).pop();
                Navigator.of(context).push(MaterialPageRoute(
                    builder: (_) => DeviceChatScreen(device: device)));
              },
            ),
            ListTile(
              leading: const Icon(Icons.message),
              title: const Text('Send private message'),
              onTap: () {
                Navigator.of(context).pop();
                _promptPrivateMessage(device);
              },
            ),
            ListTile(
              leading: const Icon(Icons.attach_file),
              title: const Text('Send file'),
              onTap: () {
                Navigator.of(context).pop();
                _pickAndSendFile(device);
              },
            ),
            ListTile(
              leading: const Icon(Icons.close),
              title: const Text('Cancel'),
              onTap: () => Navigator.of(context).pop(),
            ),
          ],
        ),
      ),
    );
  }

  Future<void> _promptPrivateMessage(Device device) async {
    final controller = TextEditingController();
    final result = await showDialog<bool>(
      context: context,
      builder: (_) => AlertDialog(
        title: Text('Message to ${device.deviceName}'),
        content: TextField(
          controller: controller,
          decoration:
              const InputDecoration(hintText: 'Type your private message'),
        ),
        actions: [
          TextButton(
              onPressed: () => Navigator.of(context).pop(false),
              child: const Text('Cancel')),
          ElevatedButton(
              onPressed: () => Navigator.of(context).pop(true),
              child: const Text('Send')),
        ],
      ),
    );

    if (result == true) {
      final text = controller.text.trim();
      if (text.isEmpty) return;
      if (!mounted) return;
      final api = Provider.of<ApiService>(context, listen: false);
      final from = _username ?? 'unknown';
      final ok =
          await api.sendMessage(from: from, to: device.deviceName, text: text);
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(ok ? 'Message sent' : 'Send failed')));
      await _fetch();
    }
  }

  Future<void> _pickAndSendFile(Device device) async {
    // ensure storage permission before picking
    final ok = await PermissionService.requestStoragePermission();
    if (!ok) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(const SnackBar(
            content: Text('Storage permission required to pick files')));
      }
      return;
    }
    final result = await FilePicker.platform.pickFiles();
    if (result == null) return;
    final filePath = result.files.single.path;
    if (filePath == null) return;
    if (!mounted) return;
    final api = Provider.of<ApiService>(context, listen: false);
    final cancel = CancelToken();
    final progress = ValueNotifier<double>(0.0);

    showDialog(
      context: context,
      barrierDismissible: false,
      builder: (_) => AlertDialog(
        title: const Text('Uploading...'),
        content: ValueListenableBuilder<double>(
          valueListenable: progress,
          builder: (context, value, _) => Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              LinearProgressIndicator(value: value),
              const SizedBox(height: 12),
              Text('${(value * 100).toStringAsFixed(0)}%'),
            ],
          ),
        ),
        actions: [
          TextButton(
            onPressed: () {
              cancel.cancel();
              Navigator.of(context).pop();
            },
            child: const Text('Cancel'),
          )
        ],
      ),
    );

    try {
      final ok = await api.uploadFileWithProgress(filePath,
          from: _username, to: device.deviceName, onProgress: (p) {
        progress.value = p;
      }, cancelToken: cancel);
      if (mounted) {
        try {
          Navigator.of(context).pop();
        } catch (_) {}
        ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text(ok ? 'File sent' : 'Upload failed')));
      }
      if (mounted) await _fetch();
    } catch (e) {
      try {
        if (mounted) Navigator.of(context).pop();
      } catch (_) {}
      if (mounted) {
        ScaffoldMessenger.of(context)
            .showSnackBar(SnackBar(content: Text('Upload error: $e')));
      }
    } finally {
      progress.dispose();
    }
  }
}
