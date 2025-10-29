# Chatridge — Offline Local WiFi Messaging (Flutter)

Chatridge is a Flutter mobile app that enables offline messaging between devices connected to an ESP32 WiFi access point.

Features included in this scaffold:
- Registration screen (username & device name)
- Polling-based chat screen (send & receive messages)
- Device list screen (discover devices)
- File picker + upload stub
- Simple persistence using SharedPreferences for registration info

ESP32 server assumptions (you must ensure your ESP32 firmware exposes these endpoints):
- Base URL: `http://192.168.4.1`
- POST `/register` with JSON {"username","deviceName","ip"}
- GET `/devices` -> JSON list of devices
- GET `/messages` -> JSON list of messages
- POST `/messages` with JSON {"from","to?","text"}
- POST `/upload` receives multipart/form-data file upload

ESP32 example firmware
----------------------
An example Arduino sketch is included at `esp32_server/chatridge_server.ino`. It provides a minimal in-memory server and demonstrates the endpoints the client scaffold expects. It's a simple demo (not production-ready). The upload endpoint in that demo is simplified — for robust file handling consider using AsyncWebServer or parsing multipart boundaries properly.

These endpoints are assumptions used to create a working client scaffold. If your ESP32 server has different endpoints or payloads, update `lib/services/api_service.dart` accordingly.

How to run
1. Install Flutter and ensure `flutter doctor` is clean.
2. Open this folder in VS Code or Android Studio.
3. Run `flutter pub get`.
4. Launch on an emulator or device that can connect to the ESP32 access point.

Notes & next steps
- This scaffold is intentionally small to give a working foundation. Add persistent message storage (sqflite/hive), encryption, better error handling, and UI improvements.
- Update API interactions in `ApiService` to match the ESP32 firmware.
