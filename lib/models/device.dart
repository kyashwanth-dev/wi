class Device {
  final String username;
  final String deviceName;
  final String ip;

  Device({required this.username, required this.deviceName, required this.ip});

  factory Device.fromJson(Map<String, dynamic> j) => Device(
        username: j['username'] as String? ?? 'unknown',
        deviceName: j['deviceName'] as String? ?? 'device',
        ip: j['ip'] as String? ?? '',
      );

  Map<String, dynamic> toJson() =>
      {'username': username, 'deviceName': deviceName, 'ip': ip};
}
