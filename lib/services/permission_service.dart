import 'dart:io';
import 'package:permission_handler/permission_handler.dart';

class PermissionService {
  /// Request storage (or photo) permission and location (needed for WiFi
  /// SSID access on Android). Returns true if all required permissions are granted.
  static Future<bool> requestStorageAndLocation() async {
    final List<Permission> perms = [];

    if (Platform.isAndroid) {
      perms.add(Permission.storage);
      perms.add(Permission.locationWhenInUse);
    } else if (Platform.isIOS) {
      perms.add(Permission.photos);
      perms.add(Permission.locationWhenInUse);
    } else {
      // desktop/web no-op
      return true;
    }

    final statuses = await perms.request();
    for (final p in perms) {
      final s = statuses[p];
      if (s == null || !s.isGranted) return false;
    }
    return true;
  }

  /// Request storage permission only (for file picking flows).
  static Future<bool> requestStoragePermission() async {
    if (Platform.isAndroid) {
      final s = await Permission.storage.request();
      if (s.isPermanentlyDenied) {
        // Can't request again programmatically; open app settings so user can enable.
        await openAppSettings();
        return false;
      }
      return s.isGranted;
    } else if (Platform.isIOS) {
      final s = await Permission.photos.request();
      return s.isGranted;
    }
    return true;
  }
}
