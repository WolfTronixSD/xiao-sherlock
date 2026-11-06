// 221B Baker Street - a security recorder for the Seeed XIAO ESP32S3 Sense
//
// Watches with the camera (motion zones), listens with the microphone, and feels with
// GPIO tripwires, an HC-SR04 ultrasonic sensor and an HLK-LD2450 radar. Every event is
// written to the case log and can save photos, an AVI video clip and a WAV audio clip
// to the SD card. Everything is configured from the web page.
//
// Board: XIAO_ESP32S3, PSRAM: OPI PSRAM, Partition: Default with spiffs (3MB APP)
#include <WiFi.h>
#include <ESPmDNS.h>
#include <time.h>
#include "app.h"

// Optional first-boot Wi-Fi: copy secrets.example.h to secrets.h (not committed to git).
// Once Wi-Fi is set in the web page, that setting wins.
#if __has_include("secrets.h")
#include "secrets.h"
#endif

static void startWifi() {
  const char *s = cfg.wifiSsid, *p = cfg.wifiPass;
#ifdef WIFI_SSID
  if (!*s) { s = WIFI_SSID; p = WIFI_PASSWORD; }
#endif
  WiFi.setHostname(cfg.hostname);
  if (*s) {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(s, p);
    Serial.printf("Wi-Fi: connecting to %s", s);
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) { delay(500); Serial.print("."); }
    Serial.println();
  }
  if (WiFi.status() == WL_CONNECTED) {
    configTzTime(cfg.tz, "pool.ntp.org", "time.nist.gov");
    MDNS.begin(cfg.hostname);
    MDNS.addService("http", "tcp", 80);
    Serial.printf("221B is open for business: http://%s  or  http://%s.local\n", WiFi.localIP().toString().c_str(), cfg.hostname);
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Baker-Street-221B");
    Serial.printf("No Wi-Fi. Join hotspot 'Baker-Street-221B' and open http://%s\n", WiFi.softAPIP().toString().c_str());
  }
}

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println("\n=== 221B Baker Street security recorder " FW_VERSION " ===");
  buildFields();
  loadConfig();
  // The clock survives a software restart, so set the time zone now, not only when Wi-Fi is up,
  // or files saved right after a restart get UTC names.
  setenv("TZ", cfg.tz, 1);
  tzset();

  if (!cameraInit()) Serial.println("Camera init FAILED");
  if (mountSD()) Serial.printf("SD card OK, %llu MB\n", SD.cardSize() / 1048576);
  else Serial.println("SD card not found - nothing will be saved");
  if (!audioInit()) Serial.println("Microphone init FAILED");

  startWifi();
  webStart();
  cameraStart();
  audioStart();
  sensorsStart();
  telegramStart();
  presenceStart();
  logEvent("start", "Holmes", String("on duty, ") + (cfg.armed ? "ARMED" : "disarmed") + ", firmware " FW_VERSION);
}

void loop() {
  static uint32_t lastFlush = 0;
  webLoop();
  if (millis() - lastFlush > 1000) { lastFlush = millis(); flushEvents(); }
  delay(2);
}
