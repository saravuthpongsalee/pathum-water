#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <stdlib.h>
#include <time.h>
#include "cyd_display.h"
#include "sarabun14_fixed.h"

// GitHub Actions converts the two PUBLIC HLS cameras into small JPEGs.
// ESP32-CYD downloads only pictures. No Windows program or private camera key.
// This is a periodically updated still image, not a live flood alarm.
static const size_t MAX_JPEG = 60000;
static const uint32_t VALID_CLOCK_AFTER = 1700000000UL;
static const uint32_t MAX_AGE_SEC = 15UL * 60UL;

LGFX lcd;
Preferences settings;
WebServer portal(80);
DNSServer dns;
String savedSsid, savedPassword, cloudBase, portalName, portalPassword;
uint8_t selectedCamera = 0, zoomLevel = 1;
bool setupMode = false, havePicture = false, touchedBefore = false, holdHeader = false;
uint32_t holdStarted = 0, restartAt = 0, nextFrameAt = 0, lastClockDraw = 0;
uint32_t nextReconnectAt = 0;
int lastSecond = -1;
uint32_t displayedEpoch = 0;

static String htmlEscape(const String& input) {
  String out;
  for (size_t i = 0; i < input.length(); ++i) {
    switch (input[i]) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out += input[i]; break;
    }
  }
  return out;
}

static bool validCloudBase(const String& input) {
  if (input.length() < 20 || input.length() > 150 || !input.startsWith("https://")) return false;
  int slash = input.indexOf('/', 8);
  String host = input.substring(8, slash < 0 ? input.length() : slash);
  if (!host.endsWith(".github.io") || host.length() <= 10) return false;
  for (size_t i = 8; i < input.length(); ++i) {
    char c = input[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '/'))
      return false;
  }
  return true;
}

static void drawPortal() {
  lcd.fillScreen(0x0823);
  lcd.fillRoundRect(7, 7, 306, 226, 11, 0x10C7);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(0x07FF, 0x10C7);
  lcd.drawString("เมืองปทุมธานี", 160, 27);
  lcd.setTextColor(TFT_WHITE, 0x10C7);
  lcd.drawString("ตั้งค่า Wi-Fi และภาพจาก GitHub", 160, 56);
  lcd.drawString("เชื่อมต่อ Wi-Fi นี้จากมือถือ", 160, 87);
  lcd.drawString(portalName, 160, 111);
  lcd.drawString("รหัส: " + portalPassword, 160, 136);
  lcd.setTextColor(0x07FF, 0x10C7);
  lcd.drawString("เปิด 192.168.4.1", 160, 169);
  lcd.setTextColor(TFT_WHITE, 0x10C7);
  lcd.drawString("กรอกลิงก์ GitHub Pages ของคุณ", 160, 201);
}

static void startPortal() {
  if (setupMode) return;
  setupMode = true;
  uint32_t id = (uint32_t)ESP.getEfuseMac();
  char suffix[9];
  snprintf(suffix, sizeof(suffix), "%08X", id);
  portalName = String("PATHUM-") + suffix;
  portalPassword = String("PT") + suffix;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(portalName.c_str(), portalPassword.c_str());
  dns.start(53, "*", WiFi.softAPIP());
  portal.on("/", HTTP_GET, []() {
    String page = "<!doctype html><html lang='th'><meta charset='utf-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>เมืองปทุมธานี</title><style>body{background:#0c2330;color:#eef;"
      "font:18px sans-serif;max-width:520px;margin:auto;padding:18px}input{display:block;"
      "width:100%;box-sizing:border-box;padding:13px;background:#173648;color:white;"
      "border:1px solid #53838d;border-radius:8px}label{display:block;margin:22px 0}"
      "button{background:#16a6b4;color:white;padding:15px;border:0;border-radius:8px;"
      "font-size:18px;width:100%}</style><h2>เมืองปทุมธานี</h2>"
      "<p>ภาพจาก GitHub Actions ทุกประมาณ 5 นาที (อาจล่าช้า)</p>"
      "<form action='/save' method='post'><label>Wi-Fi 2.4 GHz<input name='ssid' "
      "maxlength='32' required value='";
    page += htmlEscape(savedSsid);
    page += "'></label><label>รหัส Wi-Fi<input name='password' type='password' maxlength='64' "
            "placeholder='เว้นว่างเพื่อใช้รหัสเดิม'></label>"
            "<label>ลิงก์ GitHub Pages เช่น https://myname.github.io/pathum-water"
            "<input name='cloud' maxlength='150' required value='";
    page += htmlEscape(cloudBase);
    page += "'></label><button>บันทึก</button></form>";
    portal.send(200, "text/html; charset=utf-8", page);
  });
  portal.on("/save", HTTP_POST, []() {
    String ssid = portal.arg("ssid"); ssid.trim();
    String pass = portal.arg("password");
    String cloud = portal.arg("cloud"); cloud.trim();
    while (cloud.endsWith("/")) cloud.remove(cloud.length() - 1);
    if (ssid.length() == 0 || ssid.length() > 32 || pass.length() > 64 ||
        !validCloudBase(cloud)) {
      portal.send(400, "text/plain; charset=utf-8", "ตรวจชื่อ Wi-Fi และ URL GitHub Pages");
      return;
    }
    if (pass.length() == 0 && ssid == savedSsid) pass = savedPassword;
    settings.putString("ssid", ssid);
    settings.putString("pass", pass);
    settings.putString("cloud", cloud);
    portal.send(200, "text/html; charset=utf-8",
                "<meta charset='utf-8'><h2>บันทึกแล้ว กำลังเริ่มใหม่</h2>");
    restartAt = millis() + 1200;
  });
  portal.onNotFound([]() {
    portal.sendHeader("Location", "http://192.168.4.1/", true);
    portal.send(302, "text/plain", "");
  });
  portal.begin();
  Serial.printf("[SETUP] AP=%s URL=http://192.168.4.1\n", portalName.c_str());
  drawPortal();
}

static bool connectWifi() {
  if (savedSsid.length() == 0) return false;
  Serial.printf("[WIFI] Connecting SSID=%s\n", savedSsid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(savedSsid.c_str(), savedPassword.c_str());
  uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 12000) delay(100);
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[WIFI] Failed status=%d\n", WiFi.status());
    return false;
  }
  configTime(7 * 3600, 0, "pool.ntp.org", "time.google.com");
  Serial.printf("[WIFI] Connected IP=%s RSSI=%d heap=%u\n",
                WiFi.localIP().toString().c_str(), WiFi.RSSI(), ESP.getFreeHeap());
  return true;
}

static void drawHeader(bool force = false) {
  if (setupMode) return;
  time_t now = time(nullptr);
  struct tm local;
  int second = -1;
  if (now > VALID_CLOCK_AFTER && localtime_r(&now, &local)) second = local.tm_sec;
  if (!force && second == lastSecond && millis() - lastClockDraw < 1000) return;
  lastSecond = second;
  lastClockDraw = millis();
  lcd.fillRect(0, 0, 320, 40, 0x0842);
  lcd.setTextDatum(middle_left);
  lcd.setTextColor(0x07FF, 0x0842);
  lcd.drawString("เมืองปทุมธานี", 8, 10);
  lcd.setTextColor(0xBDF7, 0x0842);
  if (second < 0) {
    lcd.drawString("รอเวลาเครือข่าย", 8, 29);
    lcd.setTextDatum(middle_right);
    lcd.drawString("--:--:--", 311, 19);
  } else {
    static const char* months[] = {"ม.ค.", "ก.พ.", "มี.ค.", "เม.ย.", "พ.ค.", "มิ.ย.",
                                  "ก.ค.", "ส.ค.", "ก.ย.", "ต.ค.", "พ.ย.", "ธ.ค."};
    lcd.drawString(String(local.tm_mday) + " " + months[local.tm_mon] + " " +
                   String(local.tm_year + 2443), 8, 29);
    char clockText[12];
    strftime(clockText, sizeof(clockText), "%H:%M:%S", &local);
    lcd.setTextDatum(middle_right);
    lcd.setTextColor(TFT_WHITE, 0x0842);
    lcd.drawString(clockText, 311, 19);
  }
}

static void drawAge() {
  if (setupMode) return;
  lcd.fillRect(4, 173, 312, 19, 0x0842);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(0xFFD0, 0x0842);
  if (displayedEpoch == 0) {
    lcd.drawString("ภาพเป็นช่วงเวลา | กำลังรอภาพ", 160, 182);
    return;
  }
  time_t now = time(nullptr);
  time_t stamp = (time_t)displayedEpoch;
  struct tm local;
  char label[8] = "--:--";
  if (localtime_r(&stamp, &local)) strftime(label, sizeof(label), "%H:%M", &local);
  uint32_t age = now >= stamp ? (uint32_t)(now - stamp) / 60 : 0;
  lcd.drawString(String("ภาพเวลา ") + label + " | " + String(age) + " นาทีที่แล้ว", 160, 182);
}

static void drawWaiting(const String& message) {
  havePicture = false;
  displayedEpoch = 0;
  lcd.fillRoundRect(4, 44, 312, 125, 8, 0x10A6);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(0x07FF, 0x10A6);
  lcd.drawString(selectedCamera == 0 ? "สะพานแดง" : "สวนเทพปทุม", 160, 70);
  lcd.setTextColor(TFT_WHITE, 0x10A6);
  lcd.drawString(message, 160, 108);
  lcd.setTextColor(0xBDF7, 0x10A6);
  lcd.drawString("ตรวจ Serial Monitor 115200", 160, 147);
  drawAge();
}

static void drawButtons() {
  if (setupMode) return;
  lcd.fillRect(0, 193, 320, 47, 0x0842);
  lcd.fillRect(270, 188, 46, 5, 0x0842);
  const char* labels[] = {"สะพานแดง", "ปทุม", "-", "+"};
  const int xs[] = {4, 112, 220, 270}, widths[] = {104, 104, 46, 46};
  for (int i = 0; i < 4; ++i) {
    uint16_t bg = i == selectedCamera ? 0x05F0 : (i < 2 ? 0x216A : 0x294D);
    lcd.fillRoundRect(xs[i], 197, widths[i], 38, 7, bg);
    lcd.setTextDatum(middle_center);
    lcd.setTextColor(TFT_WHITE, bg);
    lcd.drawString(labels[i], xs[i] + widths[i] / 2, 214);
  }
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(0xBDF7, 0x0842);
  lcd.drawString(String(zoomLevel) + "x", 293, 191);
}

static bool downloadPicture() {
  if (WiFi.status() != WL_CONNECTED || cloudBase.length() == 0) {
    Serial.printf("[CLOUD] No Wi-Fi or Pages URL: wifi=%d base=%s\n", WiFi.status(),
                  cloudBase.c_str());
    drawWaiting("ตั้งลิงก์ GitHub Pages ก่อน");
    return false;
  }
  uint8_t camera = selectedCamera + 1;
  String metaUrl = cloudBase + "/cam/" + String(camera) + ".meta?refresh=" + String(millis());
  HTTPClient meta;
  WiFiClientSecure metadataTls;
  // Public pictures only; no credentials are ever sent to this server.
  metadataTls.setInsecure();
  meta.setConnectTimeout(4500);
  meta.setTimeout(6000);
  if (!meta.begin(metadataTls, metaUrl)) {
    Serial.println("[CLOUD] Cannot open metadata URL");
    drawWaiting("เปิดข้อมูลภาพไม่ได้");
    return false;
  }
  int status = meta.GET();
  int size = meta.getSize();
  String body = (status == 200 && size >= 0 && size <= 32) ? meta.getString() : "";
  meta.end();
  uint32_t timestamp = strtoul(body.c_str(), nullptr, 10);
  time_t now = time(nullptr);
  if (status != 200 || timestamp <= VALID_CLOCK_AFTER) {
    Serial.printf("[CLOUD CAM %u] meta HTTP=%d body=%s; source unavailable\n",
                  camera, status, body.c_str());
    drawWaiting("กล้องยังไม่มีภาพใหม่");
    return false;
  }
  uint32_t ageSeconds = (uint32_t)now >= timestamp ? (uint32_t)now - timestamp : 0;
  if (now <= (time_t)VALID_CLOCK_AFTER || timestamp > (uint32_t)now + 90 ||
      ageSeconds > MAX_AGE_SEC) {
    Serial.printf("[CLOUD CAM %u] stale/invalid time: frame=%u now=%u maxAge=%u\n",
                  camera, timestamp, (uint32_t)now, MAX_AGE_SEC);
    drawWaiting(now <= (time_t)VALID_CLOCK_AFTER ? "รอนาฬิกาจากเน็ต" : "ภาพเก่าเกิน 15 นาที");
    return false;
  }
  String url = cloudBase + "/cam/" + String(camera) + "-z" + String(zoomLevel) +
               ".jpg?v=" + String(timestamp);
  WiFiClientSecure imageTls;
  imageTls.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(4500);
  http.setTimeout(7000);
  if (!http.begin(imageTls, url)) {
    Serial.printf("[CLOUD CAM %u] image begin failed\n", camera);
    return false;
  }
  int code = http.GET();
  int declared = http.getSize();
  Serial.printf("[CLOUD CAM %u] JPEG HTTP=%d length=%d URL=%s\n", camera, code,
                declared, url.c_str());
  if (code != 200 || declared <= 0 || declared > (int)MAX_JPEG) {
    http.end();
    drawWaiting("JPEG ไม่มีหรือใหญ่เกินไป");
    return false;
  }
  uint8_t* buffer = (uint8_t*)heap_caps_malloc(declared, psramFound() ?
                    (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : MALLOC_CAP_8BIT);
  if (!buffer) {
    Serial.printf("[CLOUD CAM %u] malloc failed: %d bytes, free heap=%u\n",
                  camera, declared, ESP.getFreeHeap());
    http.end();
    drawWaiting("หน่วยความจำไม่พอ");
    return false;
  }
  size_t used = 0;
  uint32_t started = millis();
  WiFiClient* stream = http.getStreamPtr();
  while (used < (size_t)declared && millis() - started < 9000UL) {
    int available = stream->available();
    if (available > 0) {
      size_t count = (size_t)available < (size_t)declared - used ?
                     (size_t)available : (size_t)declared - used;
      int received = stream->read(buffer + used, count);
      if (received > 0) used += received;
    } else if (!http.connected()) break;
    else delay(5);
  }
  http.end();
  bool valid = used == (size_t)declared && used >= 4 && buffer[0] == 0xFF &&
               buffer[1] == 0xD8 && buffer[used - 2] == 0xFF && buffer[used - 1] == 0xD9;
  bool drawn = false;
  if (valid) drawn = lcd.drawJpg(buffer, used, 4, 44, 312, 125, 0, 0, 0.975f);
  heap_caps_free(buffer);
  Serial.printf("[CLOUD CAM %u] JPEG bytes=%u valid=%d drawn=%d age=%u sec\n",
                camera, (unsigned)used, valid, drawn, ageSeconds);
  if (drawn) {
    havePicture = true;
    displayedEpoch = timestamp;
    drawAge();
  } else {
    drawWaiting("ภาพ JPEG วาดไม่สำเร็จ");
  }
  return drawn;
}

static void handleTouch() {
  uint16_t x = 0, y = 0;
  bool down = lcd.getTouch(&x, &y);
  if (!down) { touchedBefore = false; holdHeader = false; return; }
  if (setupMode) return;
  if (!touchedBefore) {
    touchedBefore = true;
    holdHeader = y < 42;
    holdStarted = millis();
    if (y >= 193) {
      if (x < 216) {
        uint8_t camera = x < 110 ? 0 : 1;
        if (camera != selectedCamera) {
          selectedCamera = camera;
          zoomLevel = 1;
          drawWaiting("กำลังโหลดภาพ...");
        }
      } else if (x >= 220 && x < 267 && zoomLevel > 1) --zoomLevel;
      else if (x >= 270 && zoomLevel < 4) ++zoomLevel;
      drawButtons();
      nextFrameAt = 0;
    }
  }
  if (holdHeader && millis() - holdStarted > 3500UL) {
    holdHeader = false;
    startPortal();
  }
}

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println("\n[BOOT] MUANG PATHUMTHANI CLOUD V2 - camera stills, no PC");
  lcd.init();
  lcd.setRotation(1);
  lcd.setBrightness(255);
  lcd.loadFont(sarabun14_fixed);
  lcd.setTextWrap(false);
  settings.begin("pathum-water", false);  // Reuse V1 Wi-Fi; never erase it.
  savedSsid = settings.getString("ssid", "");
  savedPassword = settings.getString("pass", "");
  cloudBase = settings.getString("cloud", "");
  while (cloudBase.endsWith("/")) cloudBase.remove(cloudBase.length() - 1);
  if (!validCloudBase(cloudBase)) cloudBase = "";
  lcd.fillScreen(0x0842);
  drawHeader(true);
  drawWaiting("กำลังเชื่อมต่อ Wi-Fi...");
  drawButtons();
  if (!connectWifi() || cloudBase.length() == 0) startPortal();
  else {
    Serial.printf("[CLOUD] Pages base: %s\n", cloudBase.c_str());
    drawHeader(true);
  }
}

void loop() {
  if (restartAt && (int32_t)(millis() - restartAt) >= 0) ESP.restart();
  if (setupMode) {
    dns.processNextRequest();
    portal.handleClient();
    delay(5);
    return;
  }
  handleTouch();
  drawHeader();
  if (WiFi.status() != WL_CONNECTED && (int32_t)(millis() - nextReconnectAt) >= 0) {
    nextReconnectAt = millis() + 15000UL;
    Serial.println("[WIFI] Connection lost, reconnecting");
    WiFi.reconnect();
  }
  if ((int32_t)(millis() - nextFrameAt) >= 0) {
    bool ok = downloadPicture();
    nextFrameAt = millis() + (ok ? 60000UL : 12000UL);
  }
  delay(15);
}
