class Message {
  final String from;
  final String? to;
  final String text;
  final String? ts;

  Message({required this.from, this.to, required this.text, this.ts});

  factory Message.fromJson(Map<String, dynamic> j) => Message(
        from: j['from'] as String? ?? 'unknown',
        to: j['to'] as String?,
        text: j['text'] as String? ?? '',
        ts: j['ts'] as String?,
      );

  Map<String, dynamic> toJson() => {
        'from': from,
        if (to != null) 'to': to,
        'text': text,
        if (ts != null) 'ts': ts
      };
}
