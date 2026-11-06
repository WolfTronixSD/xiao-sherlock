// Camp mode: the portable, battery-powered house. The board sleeps deeply and wakes when:
//   - the PIR (or any wake pin) fires: no Wi-Fi at all, photos/video/audio to SD, a Meshtastic
//     LoRa text goes out, then back to sleep once things are quiet again
//   - you press BOOT: the hotspot "Baker-Street-221B" opens so a phone can look at the evidence;
//     it sleeps again after some minutes without a visitor
//   - the heartbeat timer (optional): a short "still alive, battery x V" over LoRa
// Meshtastic alerts also work when camp mode is off (the radio just has to hang on a UART).
#include "app.h"
#include <WiFi.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"

static HardwareSerial meshSerial(2);
static bool meshActive = false;
static SemaphoreHandle_t meshLock = xSemaphoreCreateMutex();
static uint32_t meshLast = 0;

static const char *wakeWhy = "power on";
static int wakeKind = 0;  // 0 power on/restart, 1 wake pin, 2 BOOT button, 3 heartbeat
static volatile uint32_t deadline = 0;
static volatile bool sleepNow = false;
RTC_DATA_ATTR static uint32_t campWakes = 0;

// ---------- Meshtastic (serial module in TEXTMSG mode: every line we send becomes a channel message) ----------
void meshBegin() {
  xSemaphoreTake(meshLock, portMAX_DELAY);
  if (meshActive) { meshSerial.end(); meshActive = false; }
  if (cfg.meshEn && cfg.meshTx >= 0) {
    meshSerial.begin(cfg.meshBaud, SERIAL_8N1, cfg.meshRx, cfg.meshTx);
    meshActive = true;
  }
  xSemaphoreGive(meshLock);
}

bool meshSend(const String &text, bool force) {
  if (!meshActive) return false;
  uint32_t now = millis();
  if (!force && meshLast && now - meshLast < cfg.meshGapSec * 1000UL) return false;
  meshLast = now;
  String t = text;
  t.replace("\n", " ");
  if (t.length() > 200) t = t.substring(0, 200);  // LoRa packets are small
  xSemaphoreTake(meshLock, portMAX_DELAY);
  meshSerial.print(t);
  meshSerial.print("\n");
  meshSerial.flush();
  xSemaphoreGive(meshLock);
  logEvent("mesh", "pigeon post", "sent over LoRa: " + t);
  return true;
}

// ---------- battery ----------
int batteryMv() {
  if (cfg.battPin < 0) return -1;
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogReadMilliVolts(cfg.battPin);
  return (int)(sum / 8 * cfg.battMul / 100);
}

String battText() {
  int mv = batteryMv();
  return mv < 0 ? String("") : ", battery " + String(mv / 1000.0f, 2) + " V";
}

// ---------- waking up ----------
// Called early in setup(): decides whether this is a quiet wake (no Wi-Fi)
void campEarly() {
  switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_EXT0:  wakeKind = 1; wakeWhy = "the wake pin (PIR)"; break;
    case ESP_SLEEP_WAKEUP_EXT1:  wakeKind = 2; wakeWhy = "the BOOT button"; break;
    case ESP_SLEEP_WAKEUP_TIMER: wakeKind = 3; wakeWhy = "the heartbeat timer"; break;
    default:                     wakeKind = 0; wakeWhy = "power on or restart"; break;
  }
  if (wakeKind) {
    campWakes++;
    // pins used as wake sources stay in RTC mode after waking; hand them back to normal GPIO
    rtc_gpio_deinit(GPIO_NUM_0);
    if (cfg.campPin >= 0 && rtc_gpio_is_valid_gpio((gpio_num_t)cfg.campPin)) rtc_gpio_deinit((gpio_num_t)cfg.campPin);
  }
}

bool campQuiet() { return cfg.campEn && (wakeKind == 1 || wakeKind == 3); }

static bool wakePinActive() {
  if (cfg.campPin < 0) return false;
  return digitalRead(cfg.campPin) == (cfg.campLevel ? HIGH : LOW);
}

static void goToSleep(const String &why) {
  String next = cfg.campPin >= 0 ? pinName(cfg.campPin) + (cfg.campLevel ? " going HIGH" : " going LOW") : String("no wake pin!");
  next += ", the BOOT button";
  if (cfg.campBeatMin) next += ", or the heartbeat in " + String(cfg.campBeatMin) + " min";
  logEvent("sleep", "camp", why + " - Holmes dozes off" + battText() + ". Wakes on " + next);
  for (int i = 0; i < 20; i++) { flushEvents(); delay(100); }  // write the case log before the lights go out
  lockSD();
  SD.end();
  live.sdOK = false;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);  // keeps the wake pin pulls alive
  if (cfg.campPin >= 0 && rtc_gpio_is_valid_gpio((gpio_num_t)cfg.campPin)) {
    gpio_num_t p = (gpio_num_t)cfg.campPin;
    rtc_gpio_init(p);
    rtc_gpio_set_direction(p, RTC_GPIO_MODE_INPUT_ONLY);
    if (cfg.campLevel) { rtc_gpio_pullup_dis(p); rtc_gpio_pulldown_en(p); }
    else { rtc_gpio_pulldown_dis(p); rtc_gpio_pullup_en(p); }
    esp_sleep_enable_ext0_wakeup(p, cfg.campLevel);
  }
  rtc_gpio_init(GPIO_NUM_0);
  rtc_gpio_set_direction(GPIO_NUM_0, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pulldown_dis(GPIO_NUM_0);
  rtc_gpio_pullup_en(GPIO_NUM_0);
  esp_sleep_enable_ext1_wakeup(1ULL << 0, ESP_EXT1_WAKEUP_ANY_LOW);
  if (cfg.campBeatMin) esp_sleep_enable_timer_wakeup((uint64_t)cfg.campBeatMin * 60ULL * 1000000ULL);
  delay(50);
  esp_deep_sleep_start();
}

void campSleepNow() { sleepNow = true; }

int campSleepIn() {
  if (!cfg.campEn) return -1;
  int32_t left = (int32_t)(deadline - millis());
  return left > 0 ? left / 1000 : 0;
}

const char *campWakeWhy() { return wakeWhy; }
uint32_t campWakeCount() { return campWakes; }

static void extend(uint32_t ms) {
  uint32_t until = millis() + ms;
  if ((int32_t)(until - deadline) > 0) deadline = until;
}

// What the wake pin should do: use the tripwire wired to the same pin, else photos and video
static void reportIntruder() {
  for (int i = 0; i < MAX_INPUTS; i++)
    if (cfg.in[i].en && cfg.in[i].pin == cfg.campPin) {
      raiseEvent((Source)(SRC_INPUT1 + i), cfg.in[i].name, pinName(cfg.campPin) + " woke the house (camp mode)");
      return;
    }
  logEvent("input", "camp", pinName(cfg.campPin) + " woke the house (camp mode, no tripwire on this pin: photos and video)");
  if (cfg.armed) {
    cameraRequestPhotos("camp");
    cameraRequestVideo("camp");
    meshSend(String("221B ALARM: movement at ") + clockText() + battText(), true);
  }
}

static void campTask(void *) {
  if (cfg.campPin >= 0) pinMode(cfg.campPin, cfg.campLevel ? INPUT_PULLDOWN : INPUT_PULLUP);
  uint32_t awakeMs = cfg.campAwakeSec * 1000UL;
  if (cfg.campEn) {
    if (wakeKind == 1) deadline = millis() + awakeMs;
    else if (wakeKind == 3) deadline = millis() + 8000;
    else deadline = millis() + cfg.campHotspotMin * 60000UL;
    logEvent("wake", "camp", String("woken by ") + wakeWhy + " (wake #" + campWakes + ")" + battText());
  }
  if (cfg.campEn && wakeKind == 1) {
    delay(1500);  // let the camera find its exposure, or the first photo is green and dark
    reportIntruder();
  } else if (cfg.campEn && wakeKind == 3) {
    meshSend(String("221B all quiet at ") + clockText() + battText() + ", SD " + live.freeMB + " MB free", true);
  }
  bool wasEn = cfg.campEn;
  for (;;) {
    delay(200);
    if (!cfg.campEn) { sleepNow = false; wasEn = false; continue; }
    if (!wasEn) { wasEn = true; deadline = millis() + cfg.campHotspotMin * 60000UL; }  // just switched on from the web page
    if (sleepNow) { sleepNow = false; goToSleep("asked to sleep from the web page"); }
    if (wakePinActive()) extend(awakeMs);
    uint32_t lastWeb = webLastRequest();
    if (lastWeb && millis() - lastWeb < 120000UL) extend(120000UL - (millis() - lastWeb));
    if (live.videoRec || live.audioRec || webBusy()) continue;
    if ((int32_t)(millis() - deadline) >= 0) goToSleep(wakeKind == 1 ? "all quiet again" : wakeKind == 3 ? "heartbeat sent" : "no visitors");
  }
}

void campStart() { xTaskCreatePinnedToCore(campTask, "camp", 6144, nullptr, 2, nullptr, 1); }
