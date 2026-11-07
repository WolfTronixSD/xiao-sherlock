// External sensors: GPIO tripwires (door switch, PIR, button), HC-SR04 ultrasonic, HLK-LD2450 radar.
// To add a sensor: add its settings in config.cpp, a Source in app.h, and poll it in sensorTask().
#include "app.h"
#include <Wire.h>

static volatile bool dirty = true;
static int stable[MAX_INPUTS], last[MAX_INPUTS];  // tripwire debounce state
static uint32_t changedAt[MAX_INPUTS];
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
    stable[i] = last[i] = -1;  // read the level afresh, or it shows "?" until the pin changes
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
    if (claim(cfg.radarRx, "radar") && (cfg.radarTx < 0 || claim(cfg.radarTx, "radar"))) {  // TX is optional: we only listen
      radarSerial.begin(256000, SERIAL_8N1, cfg.radarRx, cfg.radarTx);
      radarActive = true;
    } else cfg.radarEn = false;
  }
  static bool wireOn = false;
  if (wireOn) { Wire.end(); wireOn = false; }
  live.shakeOK = -1;
  if (cfg.shakeEn) {
    if (claim(cfg.shakeSda, "seismograph") && claim(cfg.shakeScl, "seismograph")) {
      wireOn = Wire.begin(cfg.shakeSda, cfg.shakeScl, 400000);
      live.shakeOK = 0;  // shakeBegin() looks for the chip
    } else cfg.shakeEn = false;
  }
  // outputs (outputs.cpp drives them): release a pin that is no longer used, claim the new one
  static int prevIr = -1, prevSiren = -1;
  if (cfg.meshEn && !((cfg.meshTx < 0 || claim(cfg.meshTx, "LoRa radio")) && (cfg.meshRx < 0 || claim(cfg.meshRx, "LoRa radio")))) cfg.meshEn = false;
  meshBegin();
  if (cfg.battPin >= 0 && !claim(cfg.battPin, "battery")) cfg.battPin = -1;
  if (cfg.battPin >= 0) pinMode(cfg.battPin, INPUT);
  if (cfg.irPin >= 0 && !claim(cfg.irPin, "IR lamp")) cfg.irPin = -1;
  if (cfg.sirenPin >= 0 && !claim(cfg.sirenPin, "alarm bell")) cfg.sirenPin = -1;
  for (int *prev : {&prevIr, &prevSiren}) {
    int now = prev == &prevIr ? cfg.irPin : cfg.sirenPin;
    if (*prev >= 0 && *prev != now) {
      bool reused = false;
      for (int p : used) reused |= p == *prev;
      if (!reused) pinMode(*prev, INPUT);
    }
    *prev = now;
  }
  if (cfg.irPin >= 0) { digitalWrite(cfg.irPin, LOW); pinMode(cfg.irPin, OUTPUT); }
  if (cfg.sirenPin >= 0) { digitalWrite(cfg.sirenPin, cfg.sirenActiveLow ? HIGH : LOW); pinMode(cfg.sirenPin, OUTPUT); }
  if (cfg.campEn && cfg.campPin >= 0 && (cfg.campPin == cfg.sirenPin || cfg.campPin == cfg.irPin))
    warn += "Camp wake pin " + pinName(cfg.campPin) + " is also an output (alarm bell or IR lamp): pick another wake pin. ";
  bellPinsChanged();
  live.pinWarning = warn;
  if (warn.length()) logEvent("warning", "pins", warn);
}

// ---------- GPIO tripwires ----------

static void pollInputs() {
  for (int i = 0; i < MAX_INPUTS; i++) {
    Input &in = cfg.in[i];
    if (!in.en) { stable[i] = last[i] = -1; continue; }
    int v = digitalRead(in.pin);
    if (v != last[i]) { last[i] = v; changedAt[i] = millis(); continue; }
    if (v == stable[i] || millis() - changedAt[i] < (uint32_t)cfg.debounceMs) continue;
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

// ---------- MPU6050 (the Seismograph) ----------
// Read 50 times a second. Gravity is followed slowly (about 1 s); a jolt is the difference between a reading and
// that slow gravity, tilt is the angle between the slow gravity and the resting angle taken at arming.
static const uint8_t MPU = 0x68;
static volatile bool levelWanted = true;
void shakeLevelNow() { levelWanted = true; }

static bool mpuWrite(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(MPU); Wire.write(reg); Wire.write(v);
  return Wire.endTransmission() == 0;
}

static bool mpuRead(float a[3]) {
  Wire.beginTransmission(MPU); Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(MPU, (uint8_t)6) != 6) return false;
  for (int i = 0; i < 3; i++) { int16_t v = (Wire.read() << 8) | Wire.read(); a[i] = v / 8192.0f; }  // +-4 g range
  return true;
}

static float angleDeg(const float a[3], const float b[3]) {
  float d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  float n = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]) * sqrtf(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
  if (n < 0.01f) return 0;
  return acosf(constrain(d / n, -1.0f, 1.0f)) * 57.2958f;
}

static void pollShake() {
  static uint32_t lastRead = 0, lastTry = 0, tiltSince = 0, peakAt = 0, warmUntil = 0;
  static float grav[3], rest[3];
  static bool wasArmed = false;
  static int fails = 0;
  if (!cfg.shakeEn || live.shakeOK < 0) return;
  uint32_t now = millis();
  if (live.shakeOK == 0) {  // find and wake the chip, then retry every 10 s
    if (now - lastTry < 10000 && lastTry) return;
    lastTry = now;
    if (!mpuWrite(0x6B, 0x00) || !mpuWrite(0x1A, 0x03) || !mpuWrite(0x1C, 0x08) || !mpuRead(grav)) {
      static uint32_t lastLog = 0;
      if (!lastLog || now - lastLog > 600000UL) { lastLog = now; logEvent("warning", "seismograph", "no MPU6050 found on " + pinName(cfg.shakeSda) + "/" + pinName(cfg.shakeScl) + " - check the wiring"); }
      return;
    }
    live.shakeOK = 1;
    fails = 0;
    memcpy(rest, grav, sizeof(rest));
    levelWanted = true;
    warmUntil = now + 2000;
    logEvent("sensor", "seismograph", "MPU6050 found and running");
  }
  if (now - lastRead < 20) return;
  lastRead = now;
  float a[3];
  if (!mpuRead(a)) { if (++fails > 25) { live.shakeOK = 0; logEvent("warning", "seismograph", "MPU6050 stopped answering"); } return; }
  fails = 0;
  // jolt: how far this reading is from the slow gravity
  float jx = a[0] - grav[0], jy = a[1] - grav[1], jz = a[2] - grav[2];
  float jolt = sqrtf(jx * jx + jy * jy + jz * jz);
  for (int i = 0; i < 3; i++) grav[i] += (a[i] - grav[i]) * 0.02f;
  if (jolt > live.shockG || now - peakAt > 2000) { live.shockG = jolt; peakAt = now; }
  // the resting angle: on arming, on Level now, and after start-up
  bool armedNow = cfg.armed;
  if (armedNow && !wasArmed) { levelWanted = true; warmUntil = now + 3000; }  // let the door you just closed settle
  wasArmed = armedNow;
  if (levelWanted && (int32_t)(now - warmUntil) >= 0) { memcpy(rest, grav, sizeof(rest)); levelWanted = false; }
  float tilt = angleDeg(grav, rest);
  live.tiltDeg = tilt;
  // very slow changes (heat, tyres cooling, the ground settling) are followed: about 10 minutes
  if (cfg.shakeTiltDeg > 0 && tilt < cfg.shakeTiltDeg * 0.5f)
    for (int i = 0; i < 3; i++) rest[i] += (grav[i] - rest[i]) * 0.00003f;
  if ((int32_t)(now - warmUntil) < 0 || levelWanted) return;
  if (cfg.shakeShockMg > 0 && jolt * 1000 > cfg.shakeShockMg)
    raiseEvent(SRC_SHAKE, "MPU6050", "a jolt of " + String(jolt, 2) + " g (limit " + String(cfg.shakeShockMg / 1000.0f, 2) + " g)");
  if (cfg.shakeTiltDeg > 0 && tilt > cfg.shakeTiltDeg) {
    if (!tiltSince) tiltSince = now;
    if (now - tiltSince > 1000) {
      raiseEvent(SRC_SHAKE, "MPU6050", "tilted " + String(tilt, 1) + " degrees (limit " + String(cfg.shakeTiltDeg) + ") - jacked up or towed?");
      tiltSince = now;  // the cooldown keeps it from repeating too often
    }
  } else tiltSince = 0;
}

static void sensorTask(void *) {
  for (;;) {
    if (dirty) { dirty = false; setupPins(); }
    pollInputs();
    pollRadar();
    pollSonar();
    pollShake();
    delay(5);
  }
}

void sensorsStart() {
  for (int i = 0; i < MAX_INPUTS; i++) stable[i] = last[i] = -1;
  xTaskCreatePinnedToCore(sensorTask, "sensors", 6144, nullptr, 3, nullptr, 1);
}
