// SD card, clock, case log and the event dispatcher
#include "app.h"
#include <ArduinoJson.h>
#include <time.h>

#define SD_CS_PIN 21

static SemaphoreHandle_t sdLock = xSemaphoreCreateMutex();
static SemaphoreHandle_t evLock = xSemaphoreCreateMutex();

void lockSD() { xSemaphoreTake(sdLock, portMAX_DELAY); }
void unlockSD() { xSemaphoreGive(sdLock); }
bool tryLockSD(uint32_t ms) { return xSemaphoreTake(sdLock, pdMS_TO_TICKS(ms)) == pdTRUE; }

bool mountSD() {
  SD.end();
  // 20 MHz makes directory scans 5x faster than the 4 MHz default; fall back for picky cards
  live.sdOK = SD.begin(SD_CS_PIN, SPI, 20000000, "/sd", 10) && SD.cardType() != CARD_NONE;
  if (!live.sdOK) { SD.end(); live.sdOK = SD.begin(SD_CS_PIN, SPI, 4000000, "/sd", 10) && SD.cardType() != CARD_NONE; }
  if (live.sdOK && !SD.exists(SEC_DIR)) SD.mkdir(SEC_DIR);
  return live.sdOK;
}

// Open a file; if that fails the card may have glitched or been re-inserted, so remount once and retry.
File openSD(const String &path, const char *mode) {
  File f = live.sdOK ? SD.open(path, mode) : File();
  if (!f && live.sdOK && strcmp(mode, FILE_READ)) {  // writing: the folder may not exist yet
    SD.mkdir(path.substring(0, path.lastIndexOf('/')));
    f = SD.open(path, mode);
  }
  if (!f && mountSD()) f = SD.open(path, mode);
  return f;
}

// ---------- evidence folders ----------
// New evidence goes into one folder per day (/sec/2026_11_05/...): FAT gets very slow at creating files
// in a folder with thousands of entries. Older files may still sit directly in /sec.
// openSD() creates the day folder when the first file of the day is written.

// Evidence names, new style: 2026_11_06_091801_Fri__motion-Biscuit-tin.avi (day folder /sec/2026_11_06)
//                 old style: 20261106_091801_motion-Biscuit-tin.avi        (day folder /sec/20261106)
// evidenceDay() reads either: day = "YYYYMMDD", sec = seconds since midnight, tag = what comes after the stamp.
static bool digits(const char *p, int n) { for (int i = 0; i < n; i++) if (!isdigit((unsigned char)p[i])) return false; return true; }

bool evidenceDay(const char *n, char *day, int *sec, const char **tag) {
  size_t len = strlen(n);
  const char *t;
  if (len >= 16 && digits(n, 8) && n[8] == '_' && digits(n + 9, 6) && n[15] == '_') {
    if (day) { memcpy(day, n, 8); day[8] = 0; }
    t = n + 9;
    if (tag) *tag = n + 16;
  } else if (len >= 23 && digits(n, 4) && n[4] == '_' && digits(n + 5, 2) && n[7] == '_' && digits(n + 8, 2) && n[10] == '_' &&
             digits(n + 11, 6) && n[17] == '_' && n[21] == '_' && n[22] == '_') {
    if (day) { memcpy(day, n, 4); memcpy(day + 4, n + 5, 2); memcpy(day + 6, n + 8, 2); day[8] = 0; }
    t = n + 11;
    if (tag) *tag = n + 23;
  } else return false;
  if (sec) *sec = ((t[0] - '0') * 10 + t[1] - '0') * 3600 + ((t[2] - '0') * 10 + t[3] - '0') * 60 + (t[4] - '0') * 10 + t[5] - '0';
  return true;
}

// The day folder a file belongs in ("" = straight in /sec)
static String dayFolder(const String &name) {
  char d[9];
  if (!evidenceDay(name.c_str(), d)) return "";
  if (name[4] == '_') return name.substring(0, 10);  // new style: 2026_11_06
  return String(d);                                  // old style: 20261106
}

String evidencePath(const String &name) {
  String f = dayFolder(name);
  return String(SEC_DIR "/") + (f.length() ? f + "/" : String("")) + name;
}

// Where an existing file lives (day folder or the old flat /sec). Call with the SD lock held.
String findEvidence(const String &name) {
  String f = dayFolder(name);
  if (f.length()) {
    String p = String(SEC_DIR "/") + f + "/" + name;
    if (SD.exists(p)) return p;
  }
  return String(SEC_DIR "/") + name;
}

bool timeValid() { return time(nullptr) > 1700000000; }

static String fmtTime(const char *fmt) {
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  char b[32];
  strftime(b, sizeof(b), fmt, &t);
  return b;
}

// "2026_11_06_091801_Fri_": callers add "_" + tag, giving "..._Fri__motion-Biscuit-tin"
String stamp() { return timeValid() ? fmtTime("%Y_%m_%d_%H%M%S_%a_") : "boot" + String(millis() / 1000); }
String todayKey() { return timeValid() ? fmtTime("%Y%m%d") : String(""); }
String clockText() { return timeValid() ? fmtTime("%Y-%m-%d %H:%M:%S") : "+" + String(millis() / 1000) + "s"; }

String safeTag(const String &s) {
  String o;
  for (char c : s) o += isalnum((unsigned char)c) ? c : '-';
  return o.substring(0, 20);
}

// ---------- case log ----------
#define MAX_EVENTS 40
struct Ev { String time, kind, who, detail; };
static Ev events[MAX_EVENTS];
static int evCount = 0;
static String pending;  // CSV lines waiting to be written by flushEvents()

static String csv(const String &s) {
  String o = "\"";
  for (char c : s) { if (c == '"') o += '"'; o += c; }
  return o + "\"";
}

void logEvent(const String &kind, const String &who, const String &detail) {
  Ev e{clockText(), kind, who, detail};
  Serial.printf("%s  %-7s %s: %s\n", e.time.c_str(), kind.c_str(), who.c_str(), detail.c_str());
  xSemaphoreTake(evLock, portMAX_DELAY);
  if (evCount == MAX_EVENTS) {
    for (int i = 1; i < MAX_EVENTS; i++) events[i - 1] = events[i];
    evCount--;
  }
  events[evCount++] = e;
  if (pending.length() < 6000) pending += e.time + "," + kind + "," + csv(who) + "," + csv(detail) + "\n";
  xSemaphoreGive(evLock);
}

// The last n case log lines as text, newest last ("21:58:10 Front door: went HIGH")
String eventsText(int n) {
  String o;
  xSemaphoreTake(evLock, portMAX_DELAY);
  for (int i = max(0, evCount - n); i < evCount; i++) {
    const Ev &e = events[i];
    o += (e.time.length() > 11 ? e.time.substring(11) : e.time) + " " + e.who + ": " + e.detail.substring(0, 90) + "\n";
  }
  xSemaphoreGive(evLock);
  return o.length() ? o : String("nothing logged yet");
}

// "mute": keep recording, but send no alerts until then (0 = not muted)
static uint32_t muteUntil = 0;
void muteFor(int minutes) { muteUntil = minutes > 0 ? millis() + minutes * 60000UL : 0; if (minutes > 0 && !muteUntil) muteUntil = 1; }
int muteLeftMin() { return muteUntil && (int32_t)(muteUntil - millis()) > 0 ? (int32_t)(muteUntil - millis()) / 60000 + 1 : 0; }
bool alertsMuted() { return muteLeftMin() > 0; }

void flushEvents() {
  static uint32_t lastFree = 0;
  // Called from the web loop: never wait for a busy card, or the web page stalls. Try again next second.
  if (!tryLockSD(0)) return;
  xSemaphoreTake(evLock, portMAX_DELAY);
  String lines = pending;
  pending = "";
  xSemaphoreGive(evLock);
  if (lines.length()) {
    bool fresh = !SD.exists(SEC_DIR "/caselog.csv");
    File f = openSD(SEC_DIR "/caselog.csv", FILE_APPEND);
    if (f) {
      if (fresh) f.print("time,kind,who,detail\n");
      f.print(lines);
      f.close();
    }
  }
  if (live.sdOK && (!lastFree || millis() - lastFree > 30000)) {
    live.freeMB = (SD.totalBytes() - SD.usedBytes()) / 1048576;
    live.totalMB = SD.totalBytes() / 1048576;
    lastFree = millis();
  }
  unlockSD();
}

String eventsJson(int max) {
  JsonDocument doc;
  JsonArray a = doc.to<JsonArray>();
  xSemaphoreTake(evLock, portMAX_DELAY);
  for (int i = evCount - 1; i >= 0 && max-- > 0; i--) {
    JsonObject o = a.add<JsonObject>();
    o["t"] = events[i].time; o["k"] = events[i].kind; o["w"] = events[i].who; o["d"] = events[i].detail;
  }
  xSemaphoreGive(evLock);
  String s;
  serializeJson(doc, s);
  return s;
}

// What every sensor saw at the moment of an event, for the case log
String sensorSnapshot() {
  String s = "mic " + String(live.micDb, 0) + " dB";
  for (int i = 0; i < MAX_INPUTS; i++)
    if (cfg.in[i].en) s += ", " + String(cfg.in[i].name) + "=" + (live.inputLevel[i] == 1 ? "HIGH" : live.inputLevel[i] == 0 ? "LOW" : "?");
  if (cfg.motionEn) s += ", motion " + String(live.motionGlobal) + "%";
  if (cfg.radarEn) {
    int n = 0;
    for (auto &t : live.radar) if (t.valid) n++;
    s += ", radar " + String(n) + " target(s)";
  }
  if (cfg.sonarEn) s += ", sonar " + String(live.sonarCm) + " cm";
  if (cfg.shakeEn && live.shakeOK == 1) s += ", tilt " + String(live.tiltDeg, 1) + " deg, jolt " + String(live.shockG, 2) + " g";
  if (live.light >= 0) s += ", light " + String(live.light) + (live.night ? " (night)" : "");
  if (carMv() > 0) s += ", car " + String(carMv() / 1000.0f, 1) + " V";
  return s;
}

// Arm or disarm, save it and say why in the case log (web button, timetable or phone)
void setArmed(bool on, const String &who, const String &why) {
  cfg.armed = on;
  saveConfig();
  if (!on) sirenStop("disarmed");
  logEvent("config", who, String(on ? "ARMED - the game is afoot" : "disarmed - Holmes is off duty") + (why.length() ? ", " + why : ""));
}

// ---------- the janitor ----------
// Once a minute: if the card is fuller than cleanPct, delete the oldest day (never today), and repeat.
static void janitorTask(void *) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(60000));
    if (!live.sdOK) {  // the card can come back (reseated, or slow after a restart)
      lockSD();
      bool ok = mountSD();
      if (ok) live.freeMB = (SD.totalBytes() - SD.usedBytes()) / 1048576;
      unlockSD();
      if (ok) logEvent("sd", "janitor", "SD card found again, " + String(live.freeMB) + " MB free");
    }
    if (!cfg.autoClean || !live.sdOK || !timeValid()) continue;
    for (int i = 0; i < 30; i++) {  // at most 30 days per round
      lockSD();
      uint64_t total = SD.totalBytes(), used = SD.usedBytes();
      unlockSD();
      if (!total || used * 100 < total * (uint64_t)cfg.cleanPct) break;
      String day = oldestDay();
      if (!day.length()) break;
      int n = deleteDay(day, true);
      logEvent("deleted", "janitor", String("card ") + (int)(used * 100 / total) + "% full - cleared " + day + " (" + n + " file(s))");
      if (!n) break;
    }
    lockSD();
    if (live.sdOK) live.freeMB = (SD.totalBytes() - SD.usedBytes()) / 1048576;
    unlockSD();
  }
}

void janitorStart() { xTaskCreatePinnedToCore(janitorTask, "janitor", 6144, nullptr, 1, nullptr, 0); }

// ---------- event dispatcher ----------
static uint32_t lastFire[SRC_COUNT] = {};
static uint32_t lastSeen[SRC_COUNT] = {};  // every sighting, for the two-witnesses rule

// Another sensor (not this one, not the test button) fired within witnessSec?
static int otherWitness(Source src, uint32_t now) {
  for (int s = 0; s < SRC_TEST; s++)
    if (s != src && lastSeen[s] && now - lastSeen[s] <= cfg.witnessSec * 1000UL) return s;
  return -1;
}

void raiseEvent(Source src, const String &who, const String &detail) {
  if (!cfg.armed && src != SRC_TEST) return;
  String confirmedBy;
  if (src != SRC_TEST) {
    uint32_t t = millis();
    int w = otherWitness(src, t);
    lastSeen[src] = t;
    // a quiet camp wake has only the PIR to go on, so it never waits for a second witness
    if (cfg.witnessEn && !campQuiet()) {
      if (w < 0) {
        static uint32_t lastNote[SRC_COUNT] = {};
        if (!lastNote[src] || t - lastNote[src] > 30000UL) {
          lastNote[src] = t;
          logEvent("witness", who, detail + " - one witness only, waiting " + String(cfg.witnessSec) + " s for a second sensor");
        }
        return;
      }
      confirmedBy = String(" (confirmed by ") + sourceName((Source)w) + ")";
    }
  }
  Actions a = actionsFor(src);
  String tag = safeTag(String(sourceName(src)) + "-" + who);
  if (a.audio) audioRequest(tag, cfg.audioTrigSec);  // audio keeps extending while events continue
  uint32_t now = millis();
  if (src != SRC_TEST && lastFire[src] && now - lastFire[src] < cfg.cooldownSec * 1000UL) return;
  lastFire[src] = now;
  String what;
  if (a.photo) what += "photos ";
  if (a.video) what += "video ";
  if (a.audio) what += "audio ";
  if (a.notify && telegramReady()) what += "telegram ";
  if (a.notify && cfg.meshEn) what += "lora ";
  if (a.siren) what += "alarm-bell ";
  logEvent(sourceName(src), who, detail + confirmedBy + " | " + sensorSnapshot() + (what.length() ? " | recording " + what : ""));
  mqttEvent(sourceName(src), who, detail);
  if (a.siren) sirenSound(cfg.sirenSec, String(who) + " - " + detail);
  if (a.photo) cameraRequestPhotos(tag);
  if (a.video) cameraRequestVideo(tag);
  if (alertsMuted() && src != SRC_TEST) a.notify = false;  // "mute": recorded, but no alerts
  if (a.notify && cfg.meshEn) meshSend(String("221B: ") + who + " - " + detail + " at " + clockText() + battText(), false);
  if (a.notify) telegramAlert(String("\xF0\x9F\x95\xB5 221B: ") + who + " - " + detail + "\n" + clockText() + "\n" + sensorSnapshot(), true, src == SRC_TEST);
}
