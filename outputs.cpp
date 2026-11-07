// Outputs: the Night Watch (IR lamp + black-and-white picture), the Alarm Bell (siren/buzzer/LED)
// and the Diogenes Club (MQTT with Home Assistant auto discovery).
// The pins are claimed in sensors.cpp (setupPins) so they never clash with a sensor.
#include "app.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>

// ---------- the alarm bell ----------
static portMUX_TYPE bellMux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t bellUntil = 0;

void sirenSound(int seconds, const String &why) {
  portENTER_CRITICAL(&bellMux);
  bellUntil = millis() + seconds * 1000UL;
  bool was = live.siren;
  live.siren = true;
  portEXIT_CRITICAL(&bellMux);
  if (!was) logEvent("siren", "alarm bell", "ringing for " + String(seconds) + " s: " + why +
                     (cfg.sirenPin < 0 ? " (no output pin set)" : " on " + pinName(cfg.sirenPin)));
}

void sirenStop(const String &why) {
  portENTER_CRITICAL(&bellMux);
  bool was = live.siren;
  live.siren = false;
  portEXIT_CRITICAL(&bellMux);
  if (was) logEvent("siren", "alarm bell", "silenced: " + why);
}

// A passive piezo only clicks when a pin is switched on and off, so styles 3 and 4 drive it with a
// square-wave tone (LEDC). The pin is attached to LEDC only while it sounds.
static volatile bool bellReset = false;
static int tonePin = -1;
void bellPinsChanged() { bellReset = true; }

static void toneOff() {
  if (tonePin < 0) return;
  ledcDetach(tonePin);
  pinMode(tonePin, OUTPUT);
  tonePin = -1;
}

static void bellTick() {
  bool on = false;
  uint32_t t = millis();
  if (live.siren) {
    if ((int32_t)(t - bellUntil) >= 0) sirenStop("time is up");
    else on = cfg.sirenStyle == 0 ? true : cfg.sirenStyle == 1 ? (t / 500) % 2 == 0 : cfg.sirenStyle == 2 ? (t / 150) % 2 == 0 : true;
  }
  if (bellReset) { bellReset = false; tonePin = -1; }  // pinMode() in setupPins already released LEDC
  bool piezo = cfg.sirenStyle == 3 || cfg.sirenStyle == 4;
  if (piezo && on && cfg.sirenPin >= 0) {
    uint32_t hz;
    if (cfg.sirenStyle == 3) {  // wail: sweep 1800 -> 3600 -> 1800 Hz every 1.6 s
      uint32_t ph = t % 1600;
      hz = 1800 + (ph < 800 ? ph : 1600 - ph) * 9 / 4;
    } else {                    // beeps: 2.7 kHz (a typical piezo's loudest pitch), 200 ms on, 200 ms off
      hz = (t / 200) % 2 == 0 ? 2700 : 0;
    }
    if (tonePin != cfg.sirenPin) {
      toneOff();
      // channel 6 runs on LEDC timer 3: the camera clock owns channel 0 / timer 0, and ledcAttach() would take it
      if (!ledcAttachChannel(cfg.sirenPin, 2700, 8, 6)) return;
      tonePin = cfg.sirenPin;
    }
    ledcWriteTone(tonePin, hz);
    return;
  }
  toneOff();
  if (cfg.sirenPin >= 0) digitalWrite(cfg.sirenPin, (piezo ? false : on != cfg.sirenActiveLow) ? HIGH : LOW);
}

// ---------- the night watch ----------
static int hhmmToMin(int v) { return (v / 100) * 60 + v % 100; }

static bool nightWanted() {
  static uint32_t darkSince = 0, brightSince = 0;
  switch (cfg.nightMode) {
    case 0: return false;
    case 3: return true;
    case 2: {
      if (!timeValid()) return live.night;
      time_t now = time(nullptr);
      struct tm t;
      localtime_r(&now, &t);
      int m = t.tm_hour * 60 + t.tm_min, from = hhmmToMin(cfg.nightFrom), to = hhmmToMin(cfg.nightTo);
      return from <= to ? (m >= from && m < to) : (m >= from || m < to);
    }
    default: {  // by light level, with hysteresis and a delay so a passing shadow or car lights do not flip it
      int l = live.light;
      if (l < 0) return live.night;
      uint32_t now = millis();
      darkSince = l < cfg.nightBelow ? (darkSince ? darkSince : now) : 0;
      brightSince = l > cfg.nightAbove ? (brightSince ? brightSince : now) : 0;
      if (!live.night && darkSince && now - darkSince > 10000) return true;
      if (live.night && brightSince && now - brightSince > 30000) return false;
      return live.night;
    }
  }
}

static void nightTick() {
  static uint32_t last = 0;
  static int lastMode = -1;
  if (millis() - last < 1000 && lastMode == cfg.nightMode) {
    if (cfg.irPin >= 0) digitalWrite(cfg.irPin, live.night ? HIGH : LOW);
    return;
  }
  last = millis();
  lastMode = cfg.nightMode;
  bool n = nightWanted();
  if (n != live.night) {
    live.night = n;
    cameraApplyConfig();  // switches black and white on or off
    logEvent("night", "Holmes", n ? "night watch begins (light level " + String(live.light) + ")" + (cfg.irPin >= 0 ? ", IR lamp lit" : "")
                                  : "day watch resumes (light level " + String(live.light) + ")");
  }
  if (cfg.irPin >= 0) digitalWrite(cfg.irPin, live.night ? HIGH : LOW);
}

static void outputTask(void *) {
  for (;;) {
    bellTick();
    nightTick();
    delay(25);
  }
}

// ---------- the Diogenes Club (MQTT) ----------
static WiFiClient net;
static PubSubClient mq(net);
static volatile bool mqDirty = true;
static SemaphoreHandle_t qLock = xSemaphoreCreateMutex();
static std::vector<String> evQueue;

void outputsApplyConfig() { mqDirty = true; }

static String topic(const char *leaf) { return String(cfg.mqttBase) + "/" + leaf; }

static String uid() {
  static String id;
  if (!id.length()) {
    String m = WiFi.macAddress();
    m.replace(":", "");
    id = "baker221b_" + m.substring(6);
    id.toLowerCase();
  }
  return id;
}

void mqttEvent(const String &kind, const String &who, const String &detail) {
  if (!cfg.mqttEn) return;
  JsonDocument d;
  d["time"] = clockText(); d["kind"] = kind; d["who"] = who; d["detail"] = detail;
  String s;
  serializeJson(d, s);
  xSemaphoreTake(qLock, portMAX_DELAY);
  if (evQueue.size() < 10) evQueue.push_back(s);
  xSemaphoreGive(qLock);
}

static void publishState() {
  JsonDocument d;
  d["armed"] = cfg.armed ? "ON" : "OFF";
  d["siren"] = live.siren ? "ON" : "OFF";
  d["night"] = live.night ? "ON" : "OFF";
  d["mic"] = (int)live.micDb;
  d["light"] = live.light;
  d["motion"] = live.motionGlobal;
  d["temp"] = (int)roundf(temperatureRead());
  d["rssi"] = WiFi.RSSI();
  d["freeMB"] = live.freeMB;
  d["sd"] = live.sdOK ? "ON" : "OFF";
  d["fw"] = FW_VERSION;
  String s;
  serializeJson(d, s);
  mq.publish(topic("state").c_str(), s.c_str(), true);
}

// One Home Assistant entity; extra fields are copied from 'more' (a small JSON object)
static void announce(const char *comp, const char *obj, const char *name, const char *more) {
  JsonDocument d;
  deserializeJson(d, more);
  String base = cfg.mqttBase;
  d["name"] = name;
  d["uniq_id"] = uid() + "_" + obj;
  d["avty_t"] = base + "/status";
  if (!d["stat_t"].is<const char *>() && strcmp(comp, "button")) d["stat_t"] = base + "/state";
  else if (d["stat_t"].is<const char *>()) d["stat_t"] = base + "/" + d["stat_t"].as<String>();
  if (d["cmd_t"].is<const char *>()) d["cmd_t"] = base + "/" + d["cmd_t"].as<String>();
  JsonObject dev = d["dev"].to<JsonObject>();
  dev["ids"].to<JsonArray>().add(uid());
  dev["name"] = "221B Baker Street";
  dev["mf"] = "WolfTronix";
  dev["mdl"] = "XIAO ESP32S3 Sense";
  dev["sw"] = FW_VERSION;
  if (WiFi.status() == WL_CONNECTED) dev["cu"] = "http://" + WiFi.localIP().toString();
  String s;
  serializeJson(d, s);
  String t = String("homeassistant/") + comp + "/" + uid() + "/" + obj + "/config";
  mq.publish(t.c_str(), s.c_str(), true);
}

static void announceAll() {
  announce("switch", "armed", "Armed", R"({"cmd_t":"set/armed","val_tpl":"{{ value_json.armed }}","icon":"mdi:shield-home"})");
  announce("switch", "siren", "Alarm bell", R"({"cmd_t":"set/siren","val_tpl":"{{ value_json.siren }}","icon":"mdi:alarm-light"})");
  announce("button", "test", "Summon Lestrade (test)", R"({"cmd_t":"set/test","icon":"mdi:police-badge"})");
  announce("binary_sensor", "clue", "Clue", R"({"stat_t":"clue","dev_cla":"motion","off_delay":30})");
  announce("binary_sensor", "night", "Night watch", R"({"val_tpl":"{{ value_json.night }}","icon":"mdi:weather-night"})");
  announce("sensor", "mic", "Loudness", R"({"val_tpl":"{{ value_json.mic }}","unit_of_meas":"dB","stat_cla":"measurement","icon":"mdi:ear-hearing"})");
  announce("sensor", "light", "Light level", R"({"val_tpl":"{{ value_json.light }}","stat_cla":"measurement","icon":"mdi:brightness-6"})");
  announce("sensor", "motion", "Picture changed", R"({"val_tpl":"{{ value_json.motion }}","unit_of_meas":"%","stat_cla":"measurement","icon":"mdi:motion-sensor"})");
  announce("sensor", "temp", "Chip temperature", R"({"val_tpl":"{{ value_json.temp }}","unit_of_meas":"°C","dev_cla":"temperature","stat_cla":"measurement","ent_cat":"diagnostic"})");
  announce("sensor", "rssi", "Wi-Fi signal", R"({"val_tpl":"{{ value_json.rssi }}","unit_of_meas":"dBm","dev_cla":"signal_strength","stat_cla":"measurement","ent_cat":"diagnostic"})");
  announce("sensor", "free", "SD free", R"({"val_tpl":"{{ value_json.freeMB }}","unit_of_meas":"MB","dev_cla":"data_size","ent_cat":"diagnostic","icon":"mdi:sd"})");
  announce("sensor", "event", "Last clue", R"({"stat_t":"event","val_tpl":"{{ value_json.who }}: {{ value_json.detail[:200] }}","icon":"mdi:magnify"})");
}

static void onMessage(char *t, uint8_t *payload, unsigned int len) {
  String what = String(t).substring(strlen(cfg.mqttBase) + 5);  // after "PREFIX/set/"
  String v;
  for (unsigned i = 0; i < len && i < 16; i++) v += (char)toupper(payload[i]);
  bool on = v == "ON" || v == "1" || v == "TRUE";
  if (what == "armed") {
    if (on != cfg.armed) setArmed(on, "Diogenes Club", "MQTT command");
    if (!on) sirenStop("disarmed over MQTT");
  } else if (what == "siren") {
    if (on) sirenSound(cfg.sirenSec, "switched on over MQTT");
    else sirenStop("switched off over MQTT");
  } else if (what == "test") {
    raiseEvent(SRC_TEST, "Inspector Lestrade", "test from the Diogenes Club (MQTT)");
  }
}

static void mqttTask(void *) {
  uint32_t nextTry = 0, lastState = 0, backoff = 5000;
  bool sentArmed = false, sentNight = false, sentSiren = false, warned = false;
  for (;;) {
    delay(50);
    if (mqDirty) {
      mqDirty = false;
      if (mq.connected()) { mq.publish(topic("status").c_str(), "offline", true); mq.disconnect(); }
      nextTry = 0; backoff = 5000; warned = false;
    }
    if (!cfg.mqttEn || !cfg.mqttHost[0]) { live.mqtt = -1; continue; }
    if (WiFi.status() != WL_CONNECTED) { live.mqtt = 0; continue; }
    if (!mq.connected()) {
      live.mqtt = 0;
      if ((int32_t)(millis() - nextTry) < 0) continue;
      mq.setServer(cfg.mqttHost, cfg.mqttPort);
      mq.setCallback(onMessage);
      mq.setBufferSize(1024);
      mq.setKeepAlive(30);
      mq.setSocketTimeout(5);
      String will = topic("status");
      bool ok = mq.connect(uid().c_str(), cfg.mqttUser[0] ? cfg.mqttUser : nullptr, cfg.mqttUser[0] ? cfg.mqttPass : nullptr,
                           will.c_str(), 0, true, "offline");
      if (!ok) {
        if (!warned) logEvent("warning", "Diogenes Club", String("cannot reach MQTT broker ") + cfg.mqttHost + ":" + cfg.mqttPort +
                                                          " (state " + mq.state() + "), will keep trying");
        warned = true;
        nextTry = millis() + backoff;
        backoff = backoff >= 30000 ? 60000 : backoff * 2;
        continue;
      }
      backoff = 5000;
      warned = false;
      live.mqtt = 1;
      mq.publish(will.c_str(), "online", true);
      if (cfg.mqttHa) announceAll();
      mq.subscribe(topic("set/#").c_str());
      logEvent("config", "Diogenes Club", String("connected to MQTT broker ") + cfg.mqttHost + (cfg.mqttHa ? ", announced to Home Assistant" : ""));
      lastState = 0;
    }
    mq.loop();
    // events waiting to go out
    std::vector<String> q;
    xSemaphoreTake(qLock, portMAX_DELAY);
    q.swap(evQueue);
    xSemaphoreGive(qLock);
    for (auto &e : q) {
      mq.publish(topic("event").c_str(), e.c_str());
      mq.publish(topic("clue").c_str(), "ON");
    }
    bool changed = sentArmed != cfg.armed || sentNight != live.night || sentSiren != live.siren;
    if (changed || q.size() || !lastState || millis() - lastState > 30000) {
      publishState();
      lastState = millis();
      sentArmed = cfg.armed; sentNight = live.night; sentSiren = live.siren;
    }
  }
}

void outputsStart() {
  xTaskCreatePinnedToCore(outputTask, "outputs", 4096, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(mqttTask, "mqtt", 8192, nullptr, 1, nullptr, 0);
}
