// 221B Baker Street - a security recorder for the Seeed XIAO ESP32S3 Sense
//
// Watches with the camera (motion zones), listens with the microphone, and feels with
// GPIO tripwires, an HC-SR04 ultrasonic sensor and an HLK-LD2450 radar. Every event is
// written to the case log and can save photos, an AVI video clip and a WAV audio clip
// to the SD card. Everything is configured from the web page.
//
// Board: XIAO_ESP32S3, PSRAM: OPI PSRAM, Partition: Default with spiffs (3MB APP)
#include <esp_heap_caps.h>
#include "mbedtls/platform.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <time.h>
#include <Preferences.h>
#include "esp_system.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// When the task watchdog bites, note what each core was running; it survives the restart in RTC memory
RTC_NOINIT_ATTR static char wdtTask[2][16];
RTC_NOINIT_ATTR static uint32_t wdtMagic;
extern "C" void esp_task_wdt_isr_user_handler(void) {
  for (int c = 0; c < 2; c++) {
    TaskHandle_t h = xTaskGetCurrentTaskHandleForCore(c);
    const char *n = h ? pcTaskGetName(h) : "?";
    int i = 0;
    for (; i < 15 && n[i]; i++) wdtTask[c][i] = n[i];
    wdtTask[c][i] = 0;
  }
  wdtMagic = 0xC0FFEE21;
}
#include "app.h"

// Optional first-boot Wi-Fi: copy secrets.example.h to secrets.h (not committed to git).
// Once Wi-Fi is set in the web page, that setting wins.
#if __has_include("secrets.h")
#include "secrets.h"
#endif

// Count restarts and remember why the last one happened (for the Doctor's Report)
static void countBoot() {
  Preferences p;
  p.begin("baker221b", false);
  live.boots = p.getUInt("boots", 0) + 1;
  p.putUInt("boots", live.boots);
  p.end();
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  live.resetWhy = "power on"; break;
    case ESP_RST_SW:       live.resetWhy = "software restart (web page, update or stuck-page guard)"; break;
    case ESP_RST_PANIC:    live.resetWhy = "crash"; break;
    case ESP_RST_INT_WDT:  live.resetWhy = "watchdog (interrupts blocked)"; break;
    case ESP_RST_TASK_WDT:
      live.resetWhy = "watchdog (a task hung)";
      if (wdtMagic == 0xC0FFEE21) {
        wdtTask[0][15] = wdtTask[1][15] = 0;
        live.wdtWho = String("core 0 busy with \"") + wdtTask[0] + "\", core 1 with \"" + wdtTask[1] + "\"";
        static String why;  // resetWhy is a plain C string, so keep the text alive here
        why = String("watchdog (a task hung): ") + live.wdtWho;
        live.resetWhy = why.c_str();
      }
      break;
    case ESP_RST_WDT:      live.resetWhy = "watchdog (chip hung, often a power dip)"; break;
    case ESP_RST_BROWNOUT: live.resetWhy = "power dip (brownout)"; break;
    case ESP_RST_DEEPSLEEP:live.resetWhy = "woke from sleep"; break;
    case ESP_RST_EXT:      live.resetWhy = "reset button"; break;
    case ESP_RST_USB:      live.resetWhy = "USB (upload)"; break;
    default:               live.resetWhy = "other"; break;
  }
  wdtMagic = 0;
}

static void wifiReady() {
  configTzTime(cfg.tz, "pool.ntp.org", "time.nist.gov");
  MDNS.begin(cfg.hostname);
  MDNS.addService("http", "tcp", 80);
  Serial.printf("221B is open for business: http://%s  or  http://%s.local\n", WiFi.localIP().toString().c_str(), cfg.hostname);
}

// In hotspot mode keep trying the home Wi-Fi once a minute (e.g. the router was rebooting or its
// 2.4 GHz band was off), but never while a phone is on the hotspot, because each try makes the hotspot hiccup.
static void wifiRetryTask(void *) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(60000));
    if (WiFi.status() == WL_CONNECTED || WiFi.softAPgetStationNum() > 0) continue;
    const char *s = cfg.wifiSsid, *p = cfg.wifiPass;
#ifdef WIFI_SSID
    if (!*s) { s = WIFI_SSID; p = WIFI_PASSWORD; }
#endif
    WiFi.begin(s, p);
    for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) vTaskDelay(pdMS_TO_TICKS(500));
    if (WiFi.status() != WL_CONNECTED) { WiFi.disconnect(); continue; }
    wifiReady();
    logEvent("config", "Holmes", String("back on Wi-Fi ") + s + " at " + WiFi.localIP().toString() + ", hotspot closed");
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    vTaskDelete(nullptr);
  }
}

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
    wifiReady();
  } else {
    WiFi.mode(*s ? WIFI_AP_STA : WIFI_AP);
    WiFi.softAP("Baker-Street-221B");
    Serial.printf("No Wi-Fi. Join hotspot 'Baker-Street-221B' and open http://%s\n", WiFi.softAPIP().toString().c_str());
    if (*s) xTaskCreatePinnedToCore(wifiRetryTask, "wifiretry", 4096, nullptr, 1, nullptr, 0);
  }
}

// Telegram's encryption (mbedTLS) asks for ~40 KB per connection and, in this core, only from the internal RAM.
// Give it the PSRAM instead (internal only if PSRAM is full): the internal RAM hit 0 KB with the bot listening.
static void *tlsCalloc(size_t n, size_t size) {
  void *p = heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

void setup() {
  mbedtls_platform_set_calloc_free(tlsCalloc, heap_caps_free);
  // The chip has ~300 KB of fast internal RAM but 8 MB of PSRAM. Wi-Fi, the camera and every task stack need the
  // internal RAM, so send every ordinary memory request of 2 KB or more (encryption buffers for Telegram, JSON,
  // long texts, file lists) to the PSRAM. Without this the internal RAM ran out completely during Telegram sends.
  heap_caps_malloc_extmem_enable(2048);
  Serial.begin(115200);
  delay(800);
  Serial.println("\n=== 221B Baker Street security recorder " FW_VERSION " ===");
  buildFields();
  loadConfig();
  countBoot();
  campEarly();
  // The clock survives a software restart, so set the time zone now, not only when Wi-Fi is up,
  // or files saved right after a restart get UTC names.
  setenv("TZ", cfg.tz, 1);
  tzset();

  if (!cameraInit()) Serial.println("Camera init FAILED");
  if (mountSD()) Serial.printf("SD card OK, %llu MB\n", SD.cardSize() / 1048576);
  else Serial.println("SD card not found - nothing will be saved");
  if (!audioInit()) Serial.println("Microphone init FAILED");

  if (campQuiet()) { WiFi.mode(WIFI_OFF); Serial.println("Camp mode: quiet wake, no Wi-Fi"); }
  else startWifi();
  webStart();
  cameraStart();
  audioStart();
  sensorsStart();
  telegramStart();
  presenceStart();
  outputsStart();
  irregularsStart();
  campStart();
  meshCmdStart();
  janitorStart();
  logEvent("start", "Holmes", String("on duty, ") + (cfg.armed ? "ARMED" : "disarmed") + ", firmware " FW_VERSION +
           ", restart #" + live.boots + " (" + live.resetWhy + ")");
}

void loop() {
  delay(1000);  // the web page and the case log run in their own task (see web.cpp)
}
