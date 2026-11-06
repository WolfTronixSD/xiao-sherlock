// Mrs Hudson's timetable: arm and disarm on a daily schedule, and when Holmes's phone leaves or
// comes back to the Wi-Fi.
//
// Both only act at the moment something changes (the schedule minute arrives, the phone leaves
// or returns), so arming or disarming by hand from the web page still wins until the next change.
// The phone is looked for with a ping and a knock on port 62078 (iPhones answer there even when
// they ignore pings). Phones doze on Wi-Fi, so "out" is only decided after homeAwayMin of silence.
#include "app.h"
#include <WiFi.h>
#include <time.h>
#include "ping/ping_sock.h"

static volatile int pingReplies;
static SemaphoreHandle_t pingDone = xSemaphoreCreateBinary();

static void onReply(esp_ping_handle_t, void *) { pingReplies++; }
static void onEnd(esp_ping_handle_t, void *) { xSemaphoreGive(pingDone); }

static bool pingPhone(const IPAddress &ip) {
  esp_ping_config_t pc = ESP_PING_DEFAULT_CONFIG();
  pc.target_addr.type = IPADDR_TYPE_V4;
  pc.target_addr.u_addr.ip4.addr = (uint32_t)ip;
  pc.count = 3;
  pc.interval_ms = 300;
  pc.timeout_ms = 1000;
  esp_ping_callbacks_t cb = {};
  cb.on_ping_success = onReply;
  cb.on_ping_end = onEnd;
  esp_ping_handle_t h;
  if (esp_ping_new_session(&pc, &cb, &h) != ESP_OK) return false;
  pingReplies = 0;
  xSemaphoreTake(pingDone, 0);
  esp_ping_start(h);
  xSemaphoreTake(pingDone, pdMS_TO_TICKS(6000));
  esp_ping_stop(h);
  esp_ping_delete_session(h);
  return pingReplies > 0;
}

static bool knockPhone(const IPAddress &ip) {
  NetworkClient c;
  bool ok = c.connect(ip, 62078, 1500);
  c.stop();
  return ok;
}

static int hhmmToMin(int v) { return constrain(v / 100, 0, 23) * 60 + constrain(v % 100, 0, 59); }

static void checkSchedule() {
  static int lastKey = -1;
  if (!cfg.schedEn || !timeValid()) return;
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  int m = t.tm_hour * 60 + t.tm_min, key = t.tm_yday * 1440 + m;
  if (key == lastKey) return;
  lastKey = key;
  if (m == hhmmToMin(cfg.schedArm) && !cfg.armed) setArmed(true, "timetable", "the scheduled hour has come");
  else if (m == hhmmToMin(cfg.schedDisarm) && cfg.armed) {
    // Never open up an empty house: skip the morning disarm while the phone is out.
    if (cfg.homeEn && live.phoneHome == 0) logEvent("config", "timetable", "disarm time, but Holmes is out - staying armed");
    else setArmed(false, "timetable", "the scheduled hour has come");
  }
}

static void checkPhone() {
  static uint32_t lastSeen = 0, since = 0;
  IPAddress ip;
  if (!cfg.homeEn || !ip.fromString(cfg.homeIp) || WiFi.status() != WL_CONNECTED) {
    live.phoneHome = -1;
    since = 0;
    return;
  }
  if (!since) since = millis();
  if (pingPhone(ip) || knockPhone(ip)) lastSeen = millis();
  int was = live.phoneHome, now = was;
  if (lastSeen && millis() - lastSeen < 60000) now = 1;
  else if (millis() - (lastSeen ? lastSeen : since) > cfg.homeAwayMin * 60000UL) now = 0;
  if (now == was) return;
  live.phoneHome = now;
  // Only a real change acts: right after a restart the first answer just sets the state.
  if (was == 0 && now == 1) {
    if (cfg.armed) setArmed(false, "phone", "Holmes is home");
    else logEvent("config", "phone", "Holmes is home");
  } else if (was == 1 && now == 0) {
    if (!cfg.armed) setArmed(true, "phone", "Holmes has gone out");
    else logEvent("config", "phone", "Holmes has gone out");
  }
}

static void presenceTask(void *) {
  for (;;) {
    checkSchedule();
    checkPhone();
    vTaskDelay(pdMS_TO_TICKS(20000));
  }
}

void presenceStart() { xTaskCreatePinnedToCore(presenceTask, "presence", 4096, nullptr, 1, nullptr, 0); }
