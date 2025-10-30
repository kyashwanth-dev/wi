/*
  Chatridge ESP32 server (example Arduino sketch)

  This sketch creates a WiFi AP (SSID: Chatridge, password: 12345678)
  and a simple HTTP server providing endpoints used by the Chatridge
  web UI.

  Endpoints:
  - POST /register         {"username":"...","deviceName":"..."}
  - GET  /devices          returns JSON array of devices
  - GET  /messages         returns JSON array of messages
  - POST /messages         {"from":"...","to":"...","text":"..."}
  - POST /upload           multipart (demo: accepted but not stored)
  - POST /upload_base64    {"filename":"...","content_b64":"...","from":"..."}

  This sketch embeds a small single-file HTML UI directly in flash (PROGMEM)
  so you do NOT need SPIFFS/LittleFS. It is a small demo suitable for testing
  from a laptop/phone connected to the ESP AP.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// AP configuration
const char* ssid = "Chatridge";
const char* password = "12345678";

WebServer server(80);

// Embedded single-file web UI (served from flash)
static const char index_html[] PROGMEM = R"rawliteral(
<!doctype html>
<html>
<head>
  <meta charset="utf-8" />
  <title>Chatridge Web</title>
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <style>
    body{font-family:system-ui,Segoe UI,Roboto,Arial;margin:12px}
    #messages{height:60vh;overflow:auto;border:1px solid #ddd;padding:8px}
    .msg{padding:6px;border-bottom:1px solid #f0f0f0}
  </style>
</head>
<body>
  <h2>Chatridge — Web UI</h2>
  <div>
    <strong>Username</strong> <input id="username" placeholder="Alice" />
    <strong>Device</strong> <input id="device" placeholder="laptop-1" />
    <button id="register">Register</button>
  </div>
  <hr/>
  <div id="devicesBox"><strong>Devices</strong><ul id="devices"></ul></div>
  <hr/>
  <div id="messages">
    <div id="messagesList"></div>
  </div>
  <div>
    <input id="text" placeholder="Write a message..." style="width:70%"/>
    <button id="send">Send</button>
    <input type="file" id="file" />
    <button id="upload">Upload file</button>
  </div>

  <script>
    const base = '';
    async function jsonFetch(url, opts={}) {
      const r = await fetch(url, opts);
      return r.json();
    }

    async function refreshDevices() {
      try {
        const devs = await jsonFetch('/devices');
        const ul = document.getElementById('devices');
        ul.innerHTML = '';
        devs.forEach(d => {
          const li = document.createElement('li');
          li.textContent = `${d.username} (${d.deviceName})`;
          ul.appendChild(li);
        });
      } catch(e) { console.error(e); }
    }

    async function refreshMessages() {
      try {
        const msgs = await jsonFetch('/messages');
        const list = document.getElementById('messagesList');
        list.innerHTML = '';
        msgs.forEach(m => {
          const div = document.createElement('div');
          div.className = 'msg';
          div.textContent = `${m.from}${m.to?(' -> '+m.to):''}: ${m.text}`;
          list.appendChild(div);
        });
      } catch(e) { console.error(e); }
    }

    document.getElementById('register').addEventListener('click', async () => {
      const username = document.getElementById('username').value || 'web';
      const device = document.getElementById('device').value || 'laptop';
      await fetch('/register', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({username, deviceName: device})});
      await refreshDevices();
    });

    document.getElementById('send').addEventListener('click', async () => {
      const from = document.getElementById('username').value || 'web';
      const text = document.getElementById('text').value;
      await fetch('/messages', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({from, text})});
      document.getElementById('text').value = '';
      await refreshMessages();
    });

    document.getElementById('upload').addEventListener('click', async () => {
      const fileInput = document.getElementById('file');
      if (!fileInput.files || !fileInput.files[0]) return alert('Pick file first');
      const f = fileInput.files[0];
      // Use base64 JSON upload to avoid multipart parsing complexity on the ESP
      const arr = await f.arrayBuffer();
      const b64 = btoa(String.fromCharCode(...new Uint8Array(arr)));
      await fetch('/upload_base64', {
        method: 'POST',
        headers: {'Content-Type':'application/json'},
        body: JSON.stringify({filename: f.name, content_b64: b64, from: document.getElementById('username').value})
      });
      await refreshMessages();
    });

    // start polling
    setInterval(refreshDevices, 4000);
    setInterval(refreshMessages, 2000);
    refreshDevices();
    refreshMessages();
  </script>
</body>
</html>
)rawliteral";

// Data structures
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

#define MAX_DEVICES 32
#define MAX_MESSAGES 256
static Device devices[MAX_DEVICES];
static int devicesCount = 0;
static Message messages[MAX_MESSAGES];
static int messagesCount = 0;

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

// Upload handlers (no filesystem): accept and acknowledge uploads
void handleUpload() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"error\":\"method not allowed\"}");
    return;
  }
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"no body\"}");
    return;
  }
  String body = server.arg("plain");
  // For demo we don't parse multipart here; just record a placeholder message
  String from = server.arg("from");
  String to = server.arg("to");
  String text = "[file received]";
  String ts = String(millis());
  if (messagesCount < MAX_MESSAGES) {
    messages[messagesCount++] = {from, to, text, ts};
  }
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handleUploadBase64() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"error\":\"method not allowed\"}");
    return;
  }
  String body = server.arg(0);
  StaticJsonDocument<8192> doc;
  auto err = deserializeJson(doc, body);
  if (err) {
    server.send(400, "application/json", "{\"error\":\"invalid json\"}");
    return;
  }
  const char* fname = doc["filename"] | "";
  const char* from = doc["from"] | "";
  // We won't decode/write the file in this demo; just record a message indicating receipt.
  String text = String("[file]") + String(fname);
  String ts = String(millis());
  if (messagesCount < MAX_MESSAGES) {
    messages[messagesCount++] = {String(from), "", text, ts};
  }
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handleRoot() {
  // Serve embedded HTML page (always served from flash)
  server.send_P(200, "text/html", index_html);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.softAP(ssid, password);
  Serial.println();
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/index.html", HTTP_GET, handleRoot);
  server.on("/register", HTTP_POST, handleRegister);
  server.on("/devices", HTTP_GET, handleGetDevices);
  server.on("/messages", HTTP_GET, handleGetMessages);
  server.on("/messages", HTTP_POST, handlePostMessage);
  server.on("/upload", HTTP_POST, handleUpload);
  server.on("/upload_base64", HTTP_POST, handleUploadBase64);

  server.begin();
}

void loop() {
  server.handleClient();
  delay(1);
}
