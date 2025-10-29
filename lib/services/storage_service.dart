import 'package:hive_flutter/hive_flutter.dart';
import '../models/message.dart';

class StorageService {
  static const String _boxName = 'chatridge_box';
  static const String _messagesKey = 'messages';

  static late Box _box;

  static Future<void> init() async {
    _box = await Hive.openBox(_boxName);
  }

  /// Save the whole message list (as JSON maps)
  static Future<void> saveMessages(List<Message> messages) async {
    final list = messages.map((m) => m.toJson()).toList();
    await _box.put(_messagesKey, list);
  }

  /// Load messages, returns empty list if none
  static List<Message> loadMessages() {
    final raw = _box.get(_messagesKey);
    if (raw == null) return [];
    try {
      final list = List<Map>.from(raw as List);
      return list
          .map((m) => Message.fromJson(Map<String, dynamic>.from(m)))
          .toList();
    } catch (_) {
      return [];
    }
  }

  static Future<void> addMessage(Message m) async {
    final cur = loadMessages();
    cur.add(m);
    await saveMessages(cur);
  }

  static Future<void> clearMessages() async {
    await _box.delete(_messagesKey);
  }
}
