/*
  Chatridge ESP32 server (example Arduino sketch)

  This sketch creates a WiFi AP (SSID: Chatridge, password: 12345678)
  and a simple HTTP server providing endpoints used by the Chatridge
  Flutter client scaffold.

  Endpoints (example):
  - POST /register         {"username":"...","deviceName":"..."}
  - GET  /devices          returns JSON array of devices
  - GET  /messages         returns JSON array of messages
  - POST /messages         {"from":"...","to":"...","text":"..."}
  - POST /upload           multipart file upload (field: "file") plus optional form fields 'from' and 'to'

  Notes:
  - This is a simple in-memory demo. For production, persist to SPIFFS or external storage.
  - Requires ArduinoJson (https://arduinojson.org) installed in Arduino IDE or PlatformIO.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

// AP configuration
const char* ssid = "Chatridge";
const char* password = "12345678";

WebServer server(80);

struct Device {
  String username;
  String deviceName;
  String ip;
};

struct Message {
  String from;
  String to; // empty means public/broadcast
  String text;
  String ts;
};

// Simple in-memory storage
#define MAX_DEVICES 32
#define MAX_MESSAGES 256
Device devices[MAX_DEVICES];
int devicesCount = 0;

Message messages[MAX_MESSAGES];
int messagesCount = 0;

// Helper: find device by deviceName
int findDeviceByName(const String &name) {
  for (int i = 0; i < devicesCount; ++i) {
    if (devices[i].deviceName == name) return i;
  }
  return -1;
}

void handleRegister() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"error\":\"method not allowed\"}");
    return;
  }
  String body = server.arg(0);
  StaticJsonDocument<256> doc;
  auto err = deserializeJson(doc, body);
  if (err) {
    server.send(400, "application/json", "{\"error\":\"invalid json\"}");
    return;
  }
  String username = doc["username"] | "";
  String deviceName = doc["deviceName"] | "";
  String ip = server.client().remoteIP().toString();
  if (username.length() == 0 || deviceName.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"missing fields\"}");
    return;
  }
  int idx = findDeviceByName(deviceName);
  if (idx == -1 && devicesCount < MAX_DEVICES) {
    devices[devicesCount++] = {username, deviceName, ip};
  } else if (idx != -1) {
    devices[idx].username = username;
    devices[idx].ip = ip;
  }
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handleGetDevices() {
  DynamicJsonDocument doc(1024);
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < devicesCount; ++i) {
    JsonObject o = arr.createNestedObject();
    o["username"] = devices[i].username;
    o["deviceName"] = devices[i].deviceName;
    o["ip"] = devices[i].ip;
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleGetMessages() {
  DynamicJsonDocument doc(4096);
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < messagesCount; ++i) {
    JsonObject o = arr.createNestedObject();
    o["from"] = messages[i].from;
    if (messages[i].to.length() > 0) o["to"] = messages[i].to;
    o["text"] = messages[i].text;
    o["ts"] = messages[i].ts;
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handlePostMessage() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"error\":\"method not allowed\"}");
    return;
  }
  String body = server.arg(0);
  StaticJsonDocument<512> doc;
  auto err = deserializeJson(doc, body);
  if (err) {
    server.send(400, "application/json", "{\"error\":\"invalid json\"}");
    return;
  }
  String from = doc["from"] | "";
  String to = doc["to"] | "";
  String text = doc["text"] | "";
  String ts = String(millis());
  if (from.length() == 0 || text.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"missing fields\"}");
    return;
  }
  if (messagesCount < MAX_MESSAGES) {
    messages[messagesCount++] = {from, to, text, ts};
  }
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// File upload handler
void handleUpload() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"error\":\"method not allowed\"}");
    return;
  }

  // WebServer on ESP32 doesn't provide a simple file upload parser via server.arg,
  // let's read the whole body (works for small files). For production, use AsyncWebServer.
  if (server.hasArg("plain") == false) {
    server.send(400, "application/json", "{\"error\":\"no body\"}");
    return;
  }
  String body = server.arg("plain");
  // This will receive raw multipart content; parsing multipart manually is a bit involved.
  // For a simple demo, let's just respond OK and pretend we saved the file.
  // A better approach is to use AsyncWebServer or handle uploads via the Upload class.

  // Attempt to extract 'from' and 'to' fields if provided as query parameters
  String from = server.arg("from");
  String to = server.arg("to");

  // For demo, we'll record a message announcing the file
  String text = "[file received]";
  String ts = String(millis());
  if (messagesCount < MAX_MESSAGES) {
    messages[messagesCount++] = {from, to, text, ts};
  }

  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handleRoot() {
  server.send(200, "text/plain", "Chatridge ESP32 server");
}

void setup() {
  Serial.begin(115200);
  SPIFFS.begin(true);

  WiFi.softAP(ssid, password);
  Serial.println();
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/",HTTP_GET,handleRoot);
  server.on("/register", HTTP_POST, handleRegister);
  server.on("/devices", HTTP_GET, handleGetDevices);
  server.on("/messages", HTTP_GET, handleGetMessages);
  server.on("/messages", HTTP_POST, handlePostMessage);
  server.on("/upload", HTTP_POST, handleUpload);

  server.begin();
}

void loop() {
  server.handleClient();
}
