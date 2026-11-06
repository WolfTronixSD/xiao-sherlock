// External sensors: GPIO tripwires (door switch, PIR, button), HC-SR04 ultrasonic, HLK-LD2450 radar.
// To add a sensor: add its settings in config.cpp, a Source in app.h, and poll it in sensorTask().
#include "app.h"

static volatile bool dirty = true;
static HardwareSerial radarSerial(1);
static bool radarActive = false;

void sensorsApplyConfig() { dirty = true; }

// Disable anything whose pins clash with an earlier sensor, and set the pins up
static void setupPins() {
  std::vector<int> used;
  String warn;
  auto claim = [&](int pin, const String &who) {
    if (pin < 0) return false;
    for (int p : used)
      if (p == pin) { warn += pinName(pin) + " is used twice (" + who + " disabled). "; return false; }
    used.push_back(pin);
    return true;
  };
  for (int i = 0; i < MAX_INPUTS; i++) {
    Input &in = cfg.in[i];
    live.inputLevel[i] = -1;
    if (!in.en) continue;
    if (!claim(in.pin, in.name)) { in.en = false; continue; }
    pinMode(in.pin, in.pull == 1 ? INPUT_PULLUP : in.pull == 2 ? INPUT_PULLDOWN : INPUT);
  }
  if (cfg.sonarEn) {
    if (claim(cfg.sonarTrig, "sonar") && claim(cfg.sonarEcho, "sonar")) {
      pinMode(cfg.sonarTrig, OUTPUT);
      digitalWrite(cfg.sonarTrig, LOW);
      pinMode(cfg.sonarEcho, INPUT);
    } else cfg.sonarEn = false;
  }
  if (radarActive) { radarSerial.end(); radarActive = false; }
  live.radarSeen = false;
  for (auto &t : live.radar) t.valid = false;
  if (cfg.radarEn) {
    if (claim(cfg.radarRx, "radar") && claim(cfg.radarTx, "radar")) {
      radarSerial.begin(256000, SERIAL_8N1, cfg.radarRx, cfg.radarTx);
      radarActive = true;
    } else cfg.radarEn = false;
  }
  live.pinWarning = warn;
  if (warn.length()) logEvent("warning", "pins", warn);
}

// ---------- GPIO tripwires ----------
static int stable[MAX_INPUTS], last[MAX_INPUTS];
static uint32_t changedAt[MAX_INPUTS];

static void pollInputs() {
  for (int i = 0; i < MAX_INPUTS; i++) {
    Input &in = cfg.in[i];
    if (!in.en) { stable[i] = last[i] = -1; continue; }
    int v = digitalRead(in.pin);
    if (v != last[i]) { last[i] = v; changedAt[i] = millis(); continue; }
    if (v == stable[i] || millis() - changedAt[i] < 50) continue;  // 50 ms debounce
    int prev = stable[i];
    stable[i] = live.inputLevel[i] = v;
    if (prev < 0) continue;  // first reading after enabling
    if (in.edge == 2 || (in.edge == 1 && v == HIGH) || (in.edge == 0 && v == LOW))
      raiseEvent((Source)(SRC_INPUT1 + i), in.name, pinName(in.pin) + " went " + (v ? "HIGH" : "LOW"));
  }
}

// ---------- HC-SR04 ----------
static void pollSonar() {
  static uint32_t lastPing = 0;
  static int closeCount = 0;
  if (!cfg.sonarEn || millis() - lastPing < 100) return;
  lastPing = millis();
  digitalWrite(cfg.sonarTrig, HIGH);
  delayMicroseconds(10);
  digitalWrite(cfg.sonarTrig, LOW);
  unsigned long us = pulseIn(cfg.sonarEcho, HIGH, 25000);
  int cm = us ? us / 58 : -1;
  live.sonarCm = cm;
  closeCount = (cm > 2 && cm < cfg.sonarCm) ? closeCount + 1 : 0;
  if (closeCount == 2) raiseEvent(SRC_SONAR, "ultrasonic", "something at " + String(cm) + " cm (limit " + String(cfg.sonarCm) + ")");
}

// ---------- HLK-LD2450 ----------
// Frame: AA FF 03 00 | 3 x 8 bytes target | 55 CC. Values: bit 15 set = positive, rest = magnitude.
static int16_t ldValue(uint16_t raw) { return (raw & 0x8000) ? (int16_t)(raw & 0x7FFF) : -(int16_t)(raw & 0x7FFF); }

static void pollRadar() {
  static uint8_t buf[30];
  static int pos = 0;
  static const uint8_t HDR[4] = {0xAA, 0xFF, 0x03, 0x00};
  static uint32_t lastFrame = 0;
  if (!cfg.radarEn) return;
  while (radarSerial.available()) {
    uint8_t b = radarSerial.read();
    if (pos < 4) { pos = (b == HDR[pos]) ? pos + 1 : (b == HDR[0] ? 1 : 0); if (pos) buf[pos - 1] = b; continue; }
    buf[pos++] = b;
    if (pos < 30) continue;
    pos = 0;
    if (buf[28] != 0x55 || buf[29] != 0xCC) continue;
    lastFrame = millis();
    live.radarSeen = true;
    String detail;
    for (int t = 0; t < 3; t++) {
      const uint8_t *p = buf + 4 + t * 8;
      uint16_t rx = p[0] | p[1] << 8, ry = p[2] | p[3] << 8, rs = p[4] | p[5] << 8;
      RadarTarget &tg = live.radar[t];
      tg.valid = rx || ry;
      tg.x = ldValue(rx);
      tg.y = ldValue(ry);
      tg.speed = ldValue(rs);
      if (tg.valid && sqrtf((float)tg.x * tg.x + (float)tg.y * tg.y) < cfg.radarMaxMm)
        detail += "target " + String(t + 1) + " at x=" + String(tg.x / 1000.0f, 2) + " m, y=" + String(tg.y / 1000.0f, 2) +
                  " m, " + String(tg.speed) + " cm/s; ";
    }
    if (detail.length()) raiseEvent(SRC_RADAR, "radar", detail);
  }
  if (live.radarSeen && millis() - lastFrame > 3000) {
    live.radarSeen = false;
    for (auto &t : live.radar) t.valid = false;
  }
}

static void sensorTask(void *) {
  for (;;) {
    if (dirty) { dirty = false; setupPins(); }
    pollInputs();
    pollRadar();
    pollSonar();
    delay(10);
  }
}

void sensorsStart() {
  for (int i = 0; i < MAX_INPUTS; i++) stable[i] = last[i] = -1;
  xTaskCreatePinnedToCore(sensorTask, "sensors", 6144, nullptr, 3, nullptr, 1);
}
