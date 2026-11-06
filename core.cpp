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
// New evidence goes into one folder per day (/sec/20261105/...): FAT gets very slow at creating files
// in a folder with thousands of entries. Older files may still sit directly in /sec.
// openSD() creates the day folder when the first file of the day is written.

static bool isDated(const String &name) {
  if (name.length() < 9 || name[8] != '_') return false;
  for (int i = 0; i < 8; i++) if (!isdigit((unsigned char)name[i])) return false;
  return true;
}

String evidencePath(const String &name) {
  if (!isDated(name)) return String(SEC_DIR "/") + name;
  return String(SEC_DIR "/") + name.substring(0, 8) + "/" + name;
}

// Where an existing file lives (day folder or the old flat /sec). Call with the SD lock held.
String findEvidence(const String &name) {
  if (isDated(name)) {
    String p = String(SEC_DIR "/") + name.substring(0, 8) + "/" + name;
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

String stamp() { return timeValid() ? fmtTime("%Y%m%d_%H%M%S") : "boot" + String(millis() / 1000); }
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
  return s;
}

// ---------- event dispatcher ----------
static uint32_t lastFire[SRC_COUNT] = {};

void raiseEvent(Source src, const String &who, const String &detail) {
  if (!cfg.armed && src != SRC_TEST) return;
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
  logEvent(sourceName(src), who, detail + " | " + sensorSnapshot() + (what.length() ? " | recording " + what : ""));
  if (a.photo) cameraRequestPhotos(tag);
  if (a.video) cameraRequestVideo(tag);
  if (a.notify) telegramAlert(String("\xF0\x9F\x95\xB5 221B: ") + who + " - " + detail + "\n" + clockText() + "\n" + sensorSnapshot(), true, src == SRC_TEST);
}
