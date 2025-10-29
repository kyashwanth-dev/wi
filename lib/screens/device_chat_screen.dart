import 'package:flutter/material.dart';
import '../models/device.dart';
import '../models/message.dart';
import '../services/storage_service.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:provider/provider.dart';
import '../services/api_service.dart';

class DeviceChatScreen extends StatefulWidget {
  final Device device;
  const DeviceChatScreen({super.key, required this.device});

  @override
  State<DeviceChatScreen> createState() => _DeviceChatScreenState();
}

class _DeviceChatScreenState extends State<DeviceChatScreen> {
  final _ctrl = TextEditingController();
  List<Message> _messages = [];
  String? _me;

  @override
  void initState() {
    super.initState();
    _load();
  }

  void _load() async {
    final all = StorageService.loadMessages();
    // get username from shared prefs
    final prefs = await SharedPreferences.getInstance();
    _me = prefs.getString('username') ?? '';
    setState(() {
      _messages = all.where((m) {
        final other = widget.device.deviceName;
        final me = _me ?? '';
        final sentByMeToOther = m.from == me && (m.to == other);
        final sentByOtherToMe =
            m.from == other && (m.to == me || m.to == null && m.from == other);
        return sentByMeToOther || sentByOtherToMe;
      }).toList();
    });
  }

  Future<void> _send() async {
    final text = _ctrl.text.trim();
    if (text.isEmpty) return;
    final api = Provider.of<ApiService>(context, listen: false);
    final from = _me ?? '';
    final to = widget.device.deviceName;
    final ok = await api.sendMessage(from: from, to: to, text: text);
    final m = Message(
        from: from, to: to, text: text, ts: DateTime.now().toIso8601String());
    await StorageService.addMessage(m);
    setState(() {
      _messages.add(m);
    });
    _ctrl.clear();
    if (ok) {
      // optionally refresh messages from server
    }
  }

  @override
  void dispose() {
    _ctrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text('Chat — ${widget.device.deviceName}')),
      body: Column(
        children: [
          Expanded(
            child: ListView.builder(
              itemCount: _messages.length,
              itemBuilder: (context, i) {
                final m = _messages[i];
                final mine = m.from == _me;
                return ListTile(
                  title: Text(m.text),
                  subtitle: Text('${m.from} • ${m.ts ?? ''}'),
                  trailing: mine ? const Icon(Icons.person) : null,
                );
              },
            ),
          ),
          SafeArea(
            child: Padding(
              padding: const EdgeInsets.all(8.0),
              child: Row(
                children: [
                  Expanded(
                      child: TextField(
                    controller: _ctrl,
                    decoration: const InputDecoration(
                        hintText: 'Type a private message'),
                  )),
                  IconButton(onPressed: _send, icon: const Icon(Icons.send))
                ],
              ),
            ),
          )
        ],
      ),
    );
  }
}
