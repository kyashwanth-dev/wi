import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:http/http.dart' as http;
import 'package:dio/dio.dart';
import '../models/message.dart';
import '../models/device.dart';

class ApiService extends ChangeNotifier {
  // Assumed ESP32 base URL
  final String baseUrl = 'http://192.168.4.1';

  Future<bool> register(
      {required String username, required String deviceName}) async {
    final url = Uri.parse('$baseUrl/register');
    final body = jsonEncode({
      'username': username,
      'deviceName': deviceName,
    });
    try {
      final resp = await http
          .post(url, headers: {'Content-Type': 'application/json'}, body: body)
          .timeout(const Duration(seconds: 10));
      return resp.statusCode == 200;
    } on TimeoutException catch (_) {
      throw Exception(
          'Connection timed out contacting $baseUrl — are you connected to the Chatridge WiFi AP?');
    } on SocketException catch (e) {
      throw Exception('Network error: ${e.message} — check WiFi connection');
    } catch (e) {
      rethrow;
    }
  }

  Future<List<Device>> getDevices() async {
    final url = Uri.parse('$baseUrl/devices');
    final resp = await http.get(url).timeout(const Duration(seconds: 5));
    if (resp.statusCode != 200) return [];
    final data = jsonDecode(resp.body) as List<dynamic>;
    return data.map((e) => Device.fromJson(e)).toList();
  }

  Future<List<Message>> fetchMessages() async {
    final url = Uri.parse('$baseUrl/messages');
    final resp = await http.get(url).timeout(const Duration(seconds: 5));
    if (resp.statusCode != 200) return [];
    final data = jsonDecode(resp.body) as List<dynamic>;
    return data.map((e) => Message.fromJson(e)).toList();
  }

  Future<bool> sendMessage(
      {required String from, String? to, required String text}) async {
    final url = Uri.parse('$baseUrl/messages');
    final body =
        jsonEncode({'from': from, if (to != null) 'to': to, 'text': text});
    final resp = await http
        .post(url, headers: {'Content-Type': 'application/json'}, body: body)
        .timeout(const Duration(seconds: 5));
    return resp.statusCode == 200;
  }

  /// Upload a file to the ESP server. Optionally include `from` and `to` so
  /// the ESP can route the file to a specific device. Provides progress and
  /// supports cancellation via [cancelToken]. [onProgress] receives values
  /// between 0..1.
  Future<bool> uploadFileWithProgress(
    String path, {
    String? from,
    String? to,
    void Function(double progress)? onProgress,
    CancelToken? cancelToken,
  }) async {
    final dio = Dio();
    final file = File(path);
    if (!await file.exists()) throw Exception('file not found');
    final fileName = file.path.split(Platform.pathSeparator).last;
    final form = FormData.fromMap({
      if (from != null) 'from': from,
      if (to != null) 'to': to,
      'file': await MultipartFile.fromFile(file.path, filename: fileName),
    });

    final uri = '$baseUrl/upload';
    try {
      final resp = await dio.post(uri,
          data: form,
          options: Options(headers: {'Content-Type': 'multipart/form-data'}),
          onSendProgress: (sent, total) {
        if (total > 0 && onProgress != null) onProgress(sent / total);
      }, cancelToken: cancelToken);
      return resp.statusCode == 200 || resp.statusCode == 201;
    } catch (e) {
      // Dio throws DioException on cancel; detect cancelled uploads and
      // propagate a clearer exception so callers can handle it.
      if (e is DioException && e.type == DioExceptionType.cancel) {
        throw Exception('upload cancelled');
      }
      rethrow;
    }
  }

  /// Simple wrapper to upload file without progress callback.
  Future<bool> uploadFile(String path, {String? from, String? to}) async {
    return uploadFileWithProgress(path, from: from, to: to, onProgress: null);
  }
}
