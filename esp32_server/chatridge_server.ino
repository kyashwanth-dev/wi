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
#include <SPIFFS.h>
#include <ArduinoJson.h>

// Async web server libraries (install AsyncTCP and ESPAsyncWebServer)
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <vector>

// AP configuration
const char* ssid = "Chatridge";
const char* password = "12345678";

AsyncWebServer server(80);

// Temporary per-request body accumulation storage (simple vector for demo)
struct BodyItem {
  AsyncWebServerRequest* req;
  String body;
};
static std::vector<BodyItem> bodyStore;

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

// Helpers to manage bodyStore
static BodyItem* findBodyItem(AsyncWebServerRequest* req) {
  for (size_t i = 0; i < bodyStore.size(); ++i) {
    if (bodyStore[i].req == req) return &bodyStore[i];
  }
  return nullptr;
}

static void removeBodyItem(AsyncWebServerRequest* req) {
  for (size_t i = 0; i < bodyStore.size(); ++i) {
    if (bodyStore[i].req == req) {
      bodyStore.erase(bodyStore.begin() + i);
      return;
    }
  }
}

// Async handler uses chunked body callbacks; accumulate and parse when complete.
void register_onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  BodyItem* item = findBodyItem(request);
  if (!item && index == 0) {
    BodyItem b;
    b.req = request;
    b.body = String();
    bodyStore.push_back(b);
    item = findBodyItem(request);
  }
  if (item) {
    item->body += String((const char*)data, len);
    if (index + len == total) {
      // full body received
      StaticJsonDocument<256> doc;
      auto err = deserializeJson(doc, item->body);
      if (err) {
        request->send(400, "application/json", "{\"error\":\"invalid json\"}");
        removeBodyItem(request);
        return;
      }
  const char* _username = doc["username"] | "";
  const char* _deviceName = doc["deviceName"] | "";
  String username = String(_username);
  String deviceName = String(_deviceName);
      String ip = request->client()->remoteIP().toString();
      if (username.length() == 0 || deviceName.length() == 0) {
        request->send(400, "application/json", "{\"error\":\"missing fields\"}");
        removeBodyItem(request);
        return;
      }
      int idx = findDeviceByName(deviceName);
      if (idx == -1 && devicesCount < MAX_DEVICES) {
        devices[devicesCount++] = {username, deviceName, ip};
      } else if (idx != -1) {
        devices[idx].username = username;
        devices[idx].ip = ip;
      }
      request->send(200, "application/json", "{\"status\":\"ok\"}");
      removeBodyItem(request);
    }
  } else {
    // no item found and not index==0? ignore
    request->send(400, "application/json", "{\"error\":\"bad request\"}");
  }
}

void handleGetDevices(AsyncWebServerRequest *request) {
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
  request->send(200, "application/json", out);
}

void handleGetMessages(AsyncWebServerRequest *request) {
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
  request->send(200, "application/json", out);
}

void postMessage_onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  BodyItem* item = findBodyItem(request);
  if (!item && index == 0) {
    BodyItem b;
    b.req = request;
    b.body = String();
    bodyStore.push_back(b);
    item = findBodyItem(request);
  }
  if (item) {
    item->body += String((const char*)data, len);
    if (index + len == total) {
      StaticJsonDocument<512> doc;
      auto err = deserializeJson(doc, item->body);
      if (err) {
        request->send(400, "application/json", "{\"error\":\"invalid json\"}");
        removeBodyItem(request);
        return;
      }
  const char* _from = doc["from"] | "";
  const char* _to = doc["to"] | "";
  const char* _text = doc["text"] | "";
  String from = String(_from);
  String to = String(_to);
  String text = String(_text);
      String ts = String(millis());
      if (from.length() == 0 || text.length() == 0) {
        request->send(400, "application/json", "{\"error\":\"missing fields\"}");
        removeBodyItem(request);
        return;
      }
      if (messagesCount < MAX_MESSAGES) {
        messages[messagesCount++] = {from, to, text, ts};
      }
      request->send(200, "application/json", "{\"status\":\"ok\"}");
      removeBodyItem(request);
    }
  } else {
    request->send(400, "application/json", "{\"error\":\"bad request\"}");
  }
}

// --- Base64 decode helper ---
static const char* b64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
int b64Index(char c) {
  const char* p = strchr(b64chars, c);
  if (!p) return -1;
  return p - b64chars;
}

bool base64Decode(const String &in, std::vector<uint8_t> &out) {
  out.clear();
  int len = in.length();
  int val = 0, valb = -8;
  for (int i = 0; i < len; ++i) {
    char c = in.charAt(i);
    if (c == '=') break;
    int idx = b64Index(c);
    if (idx == -1) continue; // skip non-base64 chars (whitespace)
    val = (val << 6) + idx;
    valb += 6;
    if (valb >= 0) {
      out.push_back((uint8_t)((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return true;
}

// Handler for JSON base64 uploads: expects {"filename":"...","content_b64":"...","from":"...","to":"..."}
void upload_base64_onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  BodyItem* item = findBodyItem(request);
  if (!item && index == 0) {
    BodyItem b;
    b.req = request;
    b.body = String();
    bodyStore.push_back(b);
    item = findBodyItem(request);
  }
  if (item) {
    item->body += String((const char*)data, len);
    if (index + len == total) {
      StaticJsonDocument<8192> doc; // allow for large base64 strings; be mindful of memory
      auto err = deserializeJson(doc, item->body);
      if (err) {
        request->send(400, "application/json", "{\"error\":\"invalid json\"}");
        removeBodyItem(request);
        return;
      }
      const char* _filename = doc["filename"] | "";
      const char* _content = doc["content_b64"] | "";
      const char* _from = doc["from"] | "";
      const char* _to = doc["to"] | "";
      String filename = String(_filename);
      String content_b64 = String(_content);
      String from = String(_from);
      String to = String(_to);
      if (filename.length() == 0 || content_b64.length() == 0) {
        request->send(400, "application/json", "{\"error\":\"missing fields\"}");
        removeBodyItem(request);
        return;
      }
      // decode
      std::vector<uint8_t> bytes;
      if (!base64Decode(content_b64, bytes)) {
        request->send(400, "application/json", "{\"error\":\"invalid base64\"}");
        removeBodyItem(request);
        return;
      }
      // write to SPIFFS
      String path = String("/uploads/") + filename;
      File f = SPIFFS.open(path, FILE_WRITE);
      if (!f) {
        request->send(500, "application/json", "{\"error\":\"failed to open file\"}");
        removeBodyItem(request);
        return;
      }
      f.write(bytes.data(), bytes.size());
      f.close();
      // record message
      String text = String("[file]") + filename;
      String ts = String(millis());
      if (messagesCount < MAX_MESSAGES) {
        messages[messagesCount++] = {from, to, text, ts};
      }
      request->send(200, "application/json", "{\"status\":\"ok\"}");
      removeBodyItem(request);
    }
  } else {
    request->send(400, "application/json", "{\"error\":\"bad request\"}");
  }
}

// File upload handler using AsyncWebServer upload callback
void handleUpload_onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  static File uploadFile; // demo: single file handle; for concurrency, use a map per-request
  if (index == 0) {
    // ensure uploads dir exists (SPIFFS is flat but keep prefix)
    String path = String("/uploads/") + filename;
    Serial.printf("Start upload: %s\n", path.c_str());
    // open file for writing (overwrite if exists)
    uploadFile = SPIFFS.open(path, FILE_WRITE);
    if (!uploadFile) {
      Serial.println("Failed to open file for writing");
      return;
    }
  }
  if (len && uploadFile) {
    uploadFile.write(data, len);
  }
  if (final) {
    if (uploadFile) {
      uploadFile.close();
      Serial.printf("Upload finished: %s (%u bytes)\n", filename.c_str(), index + len);
      // Record a message announcing the file. Try to extract optional form fields 'from' and 'to'
      String from = "";
      String to = "";
      if (request->hasParam("from", true)) {
        from = request->getParam("from", true)->value();
      }
      if (request->hasParam("to", true)) {
        to = request->getParam("to", true)->value();
      }
      String text = String("[file]") + filename;
      String ts = String(millis());
      if (messagesCount < MAX_MESSAGES) {
        messages[messagesCount++] = {from, to, text, ts};
      }
    }
  }
}

// onRequest callback (called after upload completes)
void handleUpload_onRequest(AsyncWebServerRequest *request) {
  request->send(200, "application/json", "{\"status\":\"ok\"}");
}

void handleRoot(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", "Chatridge ESP32 server (async)");
}

// Empty body callback used to resolve overloads when no body accumulation is needed
void upload_onBody_dummy(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  // Intentionally empty: file upload is handled in onUpload callback
}

void setup() {
  Serial.begin(115200);
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
  }

  WiFi.softAP(ssid, password);
  Serial.println();
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Async handlers
  server.on("/", HTTP_GET, handleRoot);

  // /register expects JSON body; provide a body callback
  server.on("/register", HTTP_POST, [](AsyncWebServerRequest *request){
    // request handled in body callback
  }, NULL, register_onBody);

  server.on("/devices", HTTP_GET, handleGetDevices);

  server.on("/messages", HTTP_GET, handleGetMessages);
  server.on("/messages", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, postMessage_onBody);

  // Upload: register with onRequest and onUpload callbacks (no body-callback overload)
  server.on("/upload", HTTP_POST, handleUpload_onRequest, handleUpload_onUpload);

  server.begin();
}

void loop() {
  // AsyncWebServer is event-driven; no need to call handleClient()
  delay(1);
}
