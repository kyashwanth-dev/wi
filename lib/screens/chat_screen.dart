import 'dart:async';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/api_service.dart';
import '../models/message.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'devices_screen.dart';
import 'package:file_picker/file_picker.dart';
import '../services/storage_service.dart';
import '../services/permission_service.dart';
import 'package:dio/dio.dart';

class ChatScreen extends StatefulWidget {
  const ChatScreen({super.key});

  @override
  State<ChatScreen> createState() => _ChatScreenState();
}

class _ChatScreenState extends State<ChatScreen> {
  final _msgCtrl = TextEditingController();
  final List<Message> _messages = [];
  Timer? _pollTimer;
  String? _username;

  @override
  void initState() {
    super.initState();
    _initAsync();
  }

  Future<void> _initAsync() async {
    await _loadUsername();
    final stored = StorageService.loadMessages();
    if (mounted) {
      setState(() {
        _messages.clear();
        _messages.addAll(stored);
      });
    }
    _startPolling();
  }

  Future<void> _loadUsername() async {
    final sp = await SharedPreferences.getInstance();
    if (mounted) setState(() => _username = sp.getString('username'));
  }

  void _startPolling() {
    _pollTimer?.cancel();
    _pollTimer =
        Timer.periodic(const Duration(seconds: 2), (_) => _fetchMessages());
  }

  Future<void> _fetchMessages() async {
    final api = Provider.of<ApiService>(context, listen: false);
    try {
      final msgs = await api.fetchMessages();
      // persist and update UI
      await StorageService.saveMessages(msgs);
      if (mounted) {
        setState(() {
          _messages
            ..clear()
            ..addAll(msgs);
        });
      }
    } catch (_) {}
  }

  Future<void> _sendMessage({String? to}) async {
    final text = _msgCtrl.text.trim();
    if (text.isEmpty) return;
    final api = Provider.of<ApiService>(context, listen: false);
    final from = _username ?? 'unknown';
    final ok = await api.sendMessage(from: from, to: to, text: text);
    final msg = Message(
        from: from, to: to, text: text, ts: DateTime.now().toIso8601String());
    await StorageService.addMessage(msg);
    _msgCtrl.clear();
    if (mounted && ok) await _fetchMessages();
  }

  Future<void> _pickAndUpload() async {
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
    final api = Provider.of<ApiService>(context, listen: false);
    final cancel = CancelToken();
    final progress = ValueNotifier<double>(0.0);

    // show a progress dialog with cancel
    showDialog(
      context: context,
      barrierDismissible: false,
      builder: (context) {
        return AlertDialog(
          title: const Text('Uploading...'),
          content: ValueListenableBuilder<double>(
            valueListenable: progress,
            builder: (context, value, _) {
              return Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  LinearProgressIndicator(value: value),
                  const SizedBox(height: 12),
                  Text('${(value * 100).toStringAsFixed(0)}%'),
                ],
              );
            },
          ),
          actions: [
            TextButton(
                onPressed: () {
                  cancel.cancel();
                  Navigator.of(context).pop();
                },
                child: const Text('Cancel')),
          ],
        );
      },
    );

    try {
      await api.uploadFileWithProgress(filePath, from: _username,
          onProgress: (p) {
        progress.value = p;
      }, cancelToken: cancel);
      // Ensure widget is still mounted before using context (avoid using
      // BuildContext across async gaps).
      if (!mounted) return;
      try {
        Navigator.of(context).pop();
      } catch (_) {}
      ScaffoldMessenger.of(context)
          .showSnackBar(const SnackBar(content: Text('Upload succeeded')));
      await _fetchMessages();
    } catch (e) {
      // attempt to close dialog if still mounted
      if (mounted) {
        try {
          Navigator.of(context).pop();
        } catch (_) {}
      }
      if (!mounted) return;
      // Dio signals cancellation with DioException of type cancel.
      if (e is DioException && e.type == DioExceptionType.cancel) {
        ScaffoldMessenger.of(context)
            .showSnackBar(const SnackBar(content: Text('Upload cancelled')));
      } else {
        ScaffoldMessenger.of(context)
            .showSnackBar(SnackBar(content: Text('Upload failed: $e')));
      }
    } finally {
      progress.dispose();
    }
  }

  @override
  void dispose() {
    _pollTimer?.cancel();
    _msgCtrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Chatridge'),
        actions: [
          IconButton(
            icon: const Icon(Icons.people),
            onPressed: () => Navigator.of(context)
                .push(MaterialPageRoute(builder: (_) => const DevicesScreen())),
          ),
          IconButton(
            icon: const Icon(Icons.settings),
            onPressed: () => Navigator.of(context).pushNamed('/register'),
          ),
        ],
      ),
      body: Column(
        children: [
          Expanded(
            child: ListView.builder(
              reverse: true,
              itemCount: _messages.length,
              itemBuilder: (context, index) {
                final msg = _messages[_messages.length - 1 - index];
                return ListTile(
                  title: Text(msg.from),
                  subtitle: Text(msg.text),
                  trailing: msg.to != null
                      ? Text('→ ${msg.to!}',
                          style: const TextStyle(fontSize: 12))
                      : null,
                );
              },
            ),
          ),
          SafeArea(
            child: Padding(
              padding:
                  const EdgeInsets.symmetric(horizontal: 8.0, vertical: 8.0),
              child: Row(
                children: [
                  IconButton(
                      icon: const Icon(Icons.attach_file),
                      onPressed: _pickAndUpload),
                  Expanded(
                    child: TextField(
                      controller: _msgCtrl,
                      decoration:
                          const InputDecoration(hintText: 'Type a message'),
                      onSubmitted: (_) => _sendMessage(),
                    ),
                  ),
                  IconButton(
                      icon: const Icon(Icons.send),
                      onPressed: () => _sendMessage()),
                ],
              ),
            ),
          )
        ],
      ),
    );
  }
}
