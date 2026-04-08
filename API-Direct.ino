#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// =====================================
// KONFIGURASI PIN & WAKTU
// =====================================
#define RESET_PIN   0
#define RESET_TIME  5000

// =====================================
// KONFIGURASI ACCESS POINT
// =====================================
const char* apSSID = "ESP32-Login";
IPAddress apIP(10, 10, 10, 1);
IPAddress gateway(10, 10, 10, 1);
IPAddress subnet(255, 255, 255, 0);

// =====================================
// API CONFIG
// =====================================
const char* BASE_URL  = "http://90.90.90.20:3000";
const char* DEVICE_ID = "MONITORING-SAWIT-NODE1";

// =====================================
// OBJECT GLOBAL
// =====================================
DNSServer dnsServer;
WebServer server(80);
Preferences preferences;

// =====================================
// STATE GLOBAL
// =====================================
String ssid_terpilih = "";
String pass_terpilih = "";
String log_status = "Menunggu perintah...";
bool status_terhubung = false;

unsigned long lastSend = 0;
const unsigned long sendInterval = 5000;

// =====================================
// WATERMARK HTML
// =====================================
const String WATERMARK_HTML = R"=====(
  <div style='margin-top: 20px; padding-top: 15px; border-top: 1px solid rgba(255,255,255,0.1); font-size: 0.75rem; color: #94a3b8;'>
    <p>Dibuat oleh <b>ADN Network</b></p>
    <p style='margin-top: 5px; color: #6366f1; text-decoration: none;'>IG: @artha_sa_</p>
  </div>
)=====";

// =====================================
// HALAMAN ROOT
// =====================================
void handleRoot() {
  String html = R"=====(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>WiFi Setup</title><style>
*{box-sizing:border-box;margin:0;padding:0;}
body{font-family:'Segoe UI',sans-serif;background:#0f172a;color:#f8fafc;display:flex;justify-content:center;padding:20px;}
.card{background:rgba(255,255,255,0.05);backdrop-filter:blur(10px);border:1px solid rgba(255,255,255,0.1);padding:2rem;border-radius:20px;width:100%;max-width:400px;text-align:center;}
h2{margin-bottom:1rem;color:#6366f1;}
.scan-list{background:rgba(0,0,0,0.2);border-radius:10px;margin-bottom:1.5rem;max-height:150px;overflow-y:auto;text-align:left;border:1px solid rgba(255,255,255,0.1);}
.wifi-item{padding:10px;border-bottom:1px solid rgba(255,255,255,0.05);cursor:pointer;font-size:0.9rem;display:flex;justify-content:space-between;}
.wifi-item:hover{background:rgba(99,102,241,0.2);}
input{width:100%;padding:12px;background:rgba(0,0,0,0.3);border:1px solid rgba(255,255,255,0.1);border-radius:10px;color:white;margin-bottom:1rem;outline:none;}
.pass-box{position:relative;}
.toggle{position:absolute;right:10px;top:12px;font-size:0.7rem;color:#6366f1;cursor:pointer;font-weight:bold;}
button{width:100%;padding:12px;background:#6366f1;color:white;border:none;border-radius:10px;font-weight:bold;cursor:pointer;transition:0.3s;}
button:hover{background:#4f46e5;}
p{font-size:0.8rem;color:#94a3b8;margin-bottom:0.5rem;}
</style></head><body>
<div class="card">
  <h2>WiFi Setup</h2>
  <p>Pilih jaringan yang tersedia:</p>
  <div class="scan-list">
)=====";

  int n = WiFi.scanNetworks();
  if (n == 0) {
    html += "<div class='wifi-item'>Tidak ada WiFi ditemukan</div>";
  } else {
    for (int i = 0; i < n; i++) {
      html += "<div class='wifi-item' onclick='s(\"" + WiFi.SSID(i) + "\")'>";
      html += "<span>" + WiFi.SSID(i) + "</span>";
      html += "<span style='color:#6366f1'>" + String(WiFi.RSSI(i)) + " dBm</span></div>";
    }
  }

  html += R"=====(
  </div>
  <form action="/connect" method="POST">
    <input type="text" id="ssid" name="ssid" placeholder="SSID Terpilih" required>
    <div class="pass-box">
      <input type="password" id="p" name="pass" placeholder="Masukkan Password">
      <span class="toggle" onclick="t()">LIHAT</span>
    </div>
    <button type="submit">Hubungkan Sekarang</button>
  </form>
)=====";

  html += WATERMARK_HTML;
  html += R"=====(
</div>
<script>
function s(name){document.getElementById("ssid").value=name;document.getElementById("p").focus();}
function t(){var x=document.getElementById("p");x.type=x.type==="password"?"text":"password";}
</script></body></html>
)=====";

  server.send(200, "text/html", html);
}

// =====================================
// HALAMAN STATUS
// =====================================
void handleStatusPage() {
  String s = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><meta http-equiv='refresh' content='3'>";
  s += "<style>body{font-family:sans-serif;background:#0f172a;color:white;display:flex;align-items:center;justify-content:center;height:100vh;margin:0;}";
  s += ".card{background:rgba(255,255,255,0.05);padding:2rem;border-radius:20px;width:90%;max-width:400px;text-align:center;border:1px solid rgba(255,255,255,0.1);}";
  s += ".log{background:black;padding:15px;border-radius:10px;text-align:left;font-family:monospace;font-size:0.85rem;margin:1rem 0;color:#10b981;border-left:4px solid #6366f1;}";
  s += ".btn-finish{background:#10b981;color:white;padding:12px 20px;border-radius:10px;text-decoration:none;display:inline-block;font-weight:bold;margin-top:10px;}";
  s += "</style></head><body><div class='card'><h2>Status Log</h2><div class='log'>$ " + log_status + "</div>";

  if (status_terhubung) {
    s += "<p style='color:#10b981;margin-bottom:15px;'>✓ WiFi Berhasil Tersambung!</p>";
    s += "<a href='/finish' class='btn-finish'>Selesai & Tutup Setup</a>";
  } else if (log_status.indexOf("GAGAL") != -1) {
    s += "<p style='color:#ef4444;'>Gagal. Tekan tombol BOOT 5 detik untuk reset config.</p>";
    s += "<br><a href='/' style='color:#6366f1;text-decoration:none;'>← Coba Lagi</a>";
  } else {
    s += "<p style='color:#94a3b8'>Sedang memproses...</p>";
  }

  s += WATERMARK_HTML;
  s += "</div></body></html>";
  server.send(200, "text/html", s);
}

// =====================================
// HANDLE CONNECT
// =====================================
void handleConnect() {
  if (server.hasArg("ssid")) {
    ssid_terpilih = server.arg("ssid");
    pass_terpilih = server.arg("pass");

    log_status = "Menyimpan konfigurasi...";
    status_terhubung = false;

    preferences.begin("wifi-gate", false);
    preferences.putString("ssid", ssid_terpilih);
    preferences.putString("password", pass_terpilih);
    preferences.end();

    server.sendHeader("Location", "/status", true);
    server.send(302, "text/plain", "");
  } else {
    server.send(400, "text/plain", "SSID tidak ditemukan");
  }
}

// =====================================
// HANDLE FINISH
// =====================================
void handleFinish() {
  server.send(200, "text/html", "Setup selesai. Access Point dimatikan.");
  delay(1000);
  WiFi.softAPdisconnect(true);
}

// =====================================
// RESET BUTTON
// =====================================
void checkResetButton() {
  if (digitalRead(RESET_PIN) == LOW) {
    unsigned long st = millis();
    while (digitalRead(RESET_PIN) == LOW) {
      delay(10);
    }

    if (millis() - st > RESET_TIME) {
      preferences.begin("wifi-gate", false);
      preferences.clear();
      preferences.end();

      Serial.println("Config WiFi dihapus. Restart...");
      delay(1000);
      ESP.restart();
    }
  }
}

// =====================================
// COBA KONEK KE WIFI
// =====================================
void tryConnect() {
  if (ssid_terpilih == "") return;

  log_status = "Menghubungkan ke " + ssid_terpilih + "...";
  status_terhubung = false;

  WiFi.disconnect(true, true);
  delay(500);

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid_terpilih.c_str(), pass_terpilih.c_str());

  int c = 0;
  while (WiFi.status() != WL_CONNECTED && c < 20) {
    delay(500);
    c++;
    server.handleClient();
    dnsServer.processNextRequest();
  }

  if (WiFi.status() == WL_CONNECTED) {
    log_status = "SUKSES! IP: " + WiFi.localIP().toString();
    status_terhubung = true;
    Serial.println(log_status);
  } else {
    log_status = "KONEKSI GAGAL!";
    status_terhubung = false;
    Serial.println(log_status);
  }
}

// =====================================
// TEST GET API
// =====================================
void testGET() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi belum connected");
    return;
  }

  HTTPClient http;
  String url = String(BASE_URL) + "/ping";

  Serial.println("GET: " + url);

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode > 0) {
    String response = http.getString();
    Serial.print("HTTP Code: ");
    Serial.println(httpCode);
    Serial.print("Response: ");
    Serial.println(response);
  } else {
    Serial.print("GET gagal, error: ");
    Serial.println(http.errorToString(httpCode));
  }

  http.end();
}

// =====================================
// HEARTBEAT POST
// =====================================
void sendHeartbeat() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi terputus. Coba reconnect...");
    tryConnect();
    return;
  }

  HTTPClient http;
  String url = String(BASE_URL) + "/device/heartbeat";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<256> doc;
  doc["device_id"] = DEVICE_ID;
  doc["status"] = "online";
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = WiFi.RSSI();
  doc["uptime_ms"] = millis();

  String body;
  serializeJson(doc, body);

  Serial.println("POST: " + url);
  Serial.println("Payload: " + body);

  int httpCode = http.POST(body);

  if (httpCode > 0) {
    String response = http.getString();
    Serial.print("HTTP Code: ");
    Serial.println(httpCode);
    Serial.print("Response: ");
    Serial.println(response);
  } else {
    Serial.print("POST gagal, error: ");
    Serial.println(http.errorToString(httpCode));
  }

  http.end();
}

// =====================================
// SETUP
// =====================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(RESET_PIN, INPUT_PULLUP);

  preferences.begin("wifi-gate", true);
  ssid_terpilih = preferences.getString("ssid", "");
  pass_terpilih = preferences.getString("password", "");
  preferences.end();

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(apIP, gateway, subnet);
  WiFi.softAP(apSSID);

  dnsServer.start(53, "*", apIP);

  server.on("/", handleRoot);
  server.on("/status", handleStatusPage);
  server.on("/connect", HTTP_POST, handleConnect);
  server.on("/finish", handleFinish);
  server.onNotFound([]() {
    server.sendHeader("Location", "http://10.10.10.1/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  Serial.println("Web portal aktif");
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  if (ssid_terpilih != "") {
    tryConnect();
    if (status_terhubung) {
      testGET();
    }
  }
}

// =====================================
// LOOP
// =====================================
void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  checkResetButton();

  if (ssid_terpilih != "" && !status_terhubung && log_status == "Menyimpan konfigurasi...") {
    tryConnect();
    if (status_terhubung) {
      testGET();
    }
  }

  if (status_terhubung && millis() - lastSend > sendInterval) {
    lastSend = millis();
    sendHeartbeat();
  }
}