// Web GUI and JSON API
#include "app.h"
#include <WebServer.h>
#include <WiFi.h>
#include <Update.h>
#include <sys/time.h>
#include <ArduinoJson.h>
#include "ff.h"
#include <algorithm>
#include <map>
#include <functional>
#include "web_page.h"

static WebServer server(80);

// Every page asks for the password when one is set (user name: holmes)
static bool auth() {
  if (!cfg.webPass[0] || server.authenticate("holmes", cfg.webPass)) return true;
  server.requestAuthentication(BASIC_AUTH, "221B Baker Street", "The door is locked. Mrs. Hudson does not know you.");
  return false;
}

// webWhere/webBeat feed the web watchdog (see webStart)
static volatile const char *webWhere = "idle";  // "idle" = outside a request (e.g. writing the case log)
static volatile uint32_t webBeat = 0;
static void webAlive() { webBeat = millis(); }
static volatile uint32_t lastRequest = 0;  // for camp mode: stay awake while someone is looking
uint32_t webLastRequest() { return lastRequest; }
bool webBusy() { return strcmp((const char *)webWhere, "idle") != 0; }
static void webWatchdog(void *);
static void webTask(void *);

#define ROUTE(path, method, body) server.on(path, method, [] { webWhere = path; if (auth()) { body; } })

static void sendJson(const String &s) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", s);
}

static void handleStatus() {
  JsonDocument d;
  d["armed"] = cfg.armed;
  d["time"] = clockText();
  d["uptime"] = millis() / 1000;
  d["sd"] = live.sdOK;
  d["freeMB"] = live.freeMB;
  d["mic"] = (int)live.micDb;
  d["soundDb"] = cfg.soundDb;
  d["audioRec"] = live.audioRec;
  d["videoRec"] = live.videoRec;
  d["rssi"] = WiFi.RSSI();
  d["ssid"] = WiFi.SSID();
  d["wifiFromWeb"] = cfg.wifiSsid[0] != 0;
  d["heap"] = ESP.getFreeHeap() / 1024;
  d["psram"] = ESP.getFreePsram() / 1024;
  d["minHeap"] = ESP.getMinFreeHeap() / 1024;
  d["maxBlock"] = ESP.getMaxAllocHeap() / 1024;
  d["tempC"] = (int)roundf(temperatureRead());
  d["totalMB"] = live.totalMB;
  d["boots"] = live.boots;
  d["reset"] = live.resetWhy;
  d["fw"] = FW_VERSION;
  d["phoneHome"] = live.phoneHome;
  d["warn"] = live.pinWarning + (live.saveFailed ? String("Settings could not be saved to flash: they are lost at the next restart. ") : String("")) +
              (live.wdtWho.length() ? "Last restart: the watchdog found " + live.wdtWho + " hung. " : String(""));
  d["nvsFree"] = live.nvsFree;
  d["light"] = live.light;
  d["night"] = live.night;
  d["nightMode"] = cfg.nightMode;
  d["irPin"] = cfg.irPin >= 0 ? pinName(cfg.irPin) : "";
  d["siren"] = live.siren;
  d["sirenEn"] = cfg.sirenEn;
  d["irrEn"] = cfg.irrEn;
  d["sirenLow"] = cfg.sirenActiveLow;
  d["sirenPin"] = cfg.sirenPin >= 0 ? pinName(cfg.sirenPin) : "";
  d["mqtt"] = live.mqtt;
  d["camp"] = cfg.campEn;
  d["sleepIn"] = campSleepIn();
  d["wakeWhy"] = campWakeWhy();
  d["wakes"] = campWakeCount();
  d["battMv"] = batteryMv();
  d["mesh"] = cfg.meshEn;
  d["meshPins"] = cfg.meshEn ? pinName(cfg.meshTx) + (cfg.meshRx >= 0 ? "/" + pinName(cfg.meshRx) : String("")) : String("");
  d["meshHeard"] = meshHeardSec();
  d["timeOk"] = timeValid();
  JsonArray in = d["inputs"].to<JsonArray>();
  for (int i = 0; i < MAX_INPUTS; i++) {
    JsonObject o = in.add<JsonObject>();
    o["en"] = cfg.in[i].en; o["name"] = cfg.in[i].name; o["pin"] = pinName(cfg.in[i].pin); o["level"] = live.inputLevel[i];
  }
  d["motionEn"] = cfg.motionEn;
  d["motionGlobal"] = live.motionGlobal;
  JsonArray mz = d["motion"].to<JsonArray>();
  for (int i = 0; i < MAX_ZONES; i++) mz.add(live.motionPct[i]);
  d["radarEn"] = cfg.radarEn;
  d["radarSeen"] = live.radarSeen;
  d["radarMaxMm"] = cfg.radarMaxMm;
  JsonArray rt = d["radar"].to<JsonArray>();
  for (auto &t : live.radar) {
    if (!t.valid) continue;
    JsonObject o = rt.add<JsonObject>();
    o["x"] = t.x; o["y"] = t.y; o["s"] = t.speed;
  }
  d["sonarEn"] = cfg.sonarEn;
  d["sonarCm"] = live.sonarCm;
  d["sonarLimit"] = cfg.sonarCm;
  d["shakeEn"] = cfg.shakeEn;
  d["shakeOK"] = live.shakeOK;
  d["tilt"] = (float)live.tiltDeg;
  d["shock"] = (float)live.shockG;
  d["tiltLim"] = cfg.shakeTiltDeg;
  d["shockLim"] = cfg.shakeShockMg;
  d["shakeChip"] = shakeChip();
  d["i2cBus"] = live.i2cBus;
  d["i2cScan"] = live.i2cScan;
  d["i2cStuck"] = live.i2cStuck;
  JsonArray ax = d["auxMv"].to<JsonArray>();
  for (int i = 0; i < 3; i++) ax.add(live.auxMv[i]);
  d["carMv"] = carMv();
  d["ldr"] = ldrLight();
  String s;
  serializeJson(d, s);
  sendJson(s);
}

static void handleSaveConfig() {
  String err;
  String oldSsid = cfg.wifiSsid, oldPass = cfg.wifiPass, oldHost = cfg.hostname;
  if (!applyConfigJson(server.arg("plain"), err)) { server.send(400, "text/plain", err); return; }
  saveConfig();
  cameraApplyConfig();
  sensorsApplyConfig();
  outputsApplyConfig();
  irregularsApplyConfig();
  setenv("TZ", cfg.tz, 1);
  tzset();
  bool netChanged = oldSsid != cfg.wifiSsid || oldPass != cfg.wifiPass || oldHost != cfg.hostname;
  logEvent("config", "web", String("settings changed, ") + (cfg.armed ? "ARMED" : "disarmed"));
  server.send(200, "text/plain", netChanged ? "restart" : "ok");
}

// The phone tells the house the time (camp mode has no internet clock). Only used when the clock is unset or far off.
static void handleSetTime() {
  long long ms = atoll(server.arg("ms").c_str());
  time_t now = time(nullptr);
  if (ms > 1700000000000LL && (!timeValid() || llabs((long long)now - ms / 1000) > 120)) {
    struct timeval tv = {(time_t)(ms / 1000), (suseconds_t)((ms % 1000) * 1000)};
    settimeofday(&tv, nullptr);
    logEvent("config", "web", "clock set from the visiting phone: " + clockText());
    server.send(200, "text/plain", "set");
    return;
  }
  server.send(200, "text/plain", "ok");
}

static void handleArm() {
  setArmed(server.arg("on") == "1", "web", "");
  server.send(200, "text/plain", cfg.armed ? "1" : "0");
}

static void handleCapture() {
  std::vector<uint8_t> jpg;
  if (!cameraSnapshot(jpg)) { server.send(503, "text/plain", "No picture yet"); return; }
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "image/jpeg", (const char *)jpg.data(), jpg.size());
}

// Walk /sec straight from the FatFs directory entries: name and size come for free, no file is
// opened (opening thousands of files one by one took minutes and froze the web page).
// Covers the day folders (/sec/20261105/...) and older files directly in /sec.
// fn(dayFolder or "", name, size) returns false to stop early.
// Walks every file on the card. Thousands of files take seconds, so it pauses for a tick every 32 entries:
// without that the web task hogs core 0, the idle task never runs and the task watchdog restarts the board.
// shareSD (the background librarian): every 64 entries the SD card is handed back for a moment, so recording carries on.
template <typename F> static void listSec(F fn, bool shareSD = false) {
  static FF_DIR dir, sub;
  static FILINFO fi, sfi;
  uint32_t seen = 0;
  auto breathe = [&]() {
    ++seen;
    if (shareSD && (seen & 63) == 0) { unlockSD(); vTaskDelay(2); lockSD(); }
    else if ((seen & 31) == 0) vTaskDelay(1);
  };
  for (char drv = '0'; drv <= '1'; drv++) {
    char path[] = {drv, ':', '/', 's', 'e', 'c', 0};
    if (f_opendir(&dir, path) != FR_OK) continue;
    bool go = true;
    while (go && f_readdir(&dir, &fi) == FR_OK && fi.fname[0]) {
      breathe();
      if (!(fi.fattrib & AM_DIR)) { go = fn("", fi.fname, (uint32_t)fi.fsize); continue; }
      char sp[48];
      snprintf(sp, sizeof(sp), "%s/%s", path, fi.fname);
      if (f_opendir(&sub, sp) != FR_OK) continue;
      while (go && f_readdir(&sub, &sfi) == FR_OK && sfi.fname[0]) {
        breathe();
        if (!(sfi.fattrib & AM_DIR)) go = fn(fi.fname, sfi.fname, (uint32_t)sfi.fsize);
      }
      f_closedir(&sub);
    }
    f_closedir(&dir);
    return;
  }
}

// Evidence of one day for the timeline (?day=YYYYMMDD): only that day's folder, plus old undated-folder files in /sec
#define MAX_LIST 2000
struct Entry { char name[64]; uint32_t size; };

static void listDay(const String &day, std::function<void(const char *, uint32_t)> fn) {
  static FF_DIR dir;
  static FILINFO fi;
  uint32_t seen = 0;
  char k[9];
  String newStyle = day.substring(0, 4) + "_" + day.substring(4, 6) + "_" + day.substring(6, 8);
  for (char drv = '0'; drv <= '1'; drv++) {
    String root = String(drv) + ":/sec";
    if (f_opendir(&dir, root.c_str()) != FR_OK) continue;
    while (f_readdir(&dir, &fi) == FR_OK && fi.fname[0]) {
      if ((++seen & 31) == 0) vTaskDelay(1);
      if (!(fi.fattrib & AM_DIR) && evidenceDay(fi.fname, k) && day == k) fn(fi.fname, (uint32_t)fi.fsize);
    }
    f_closedir(&dir);
    for (const String &folder : {day, newStyle}) {
      if (f_opendir(&dir, (root + "/" + folder).c_str()) != FR_OK) continue;
      while (f_readdir(&dir, &fi) == FR_OK && fi.fname[0]) {
        if ((++seen & 31) == 0) vTaskDelay(1);
        if (!(fi.fattrib & AM_DIR)) fn(fi.fname, (uint32_t)fi.fsize);
      }
      f_closedir(&dir);
    }
    return;
  }
}

static void sendEntries(Entry *list, int n, int total) {
  String s;
  s.reserve(n * 40 + 16);
  s = "[";
  for (int i = 0; i < n; i++) {
    if (i) s += ',';
    s += "[\""; s += list[i].name; s += "\","; s += list[i].size; s += ']';
  }
  s += "]";
  server.sendHeader("X-Total-Files", String(total));
  sendJson(s);
}

// ---------- the librarian ----------
// Walking thousands of files takes many seconds, far too long to keep a web request (and the browser) waiting.
// A background task walks the card and keeps two ready answers: the newest 600 files for the locker and the
// statistics. The page gets them at once; a walk only runs when someone looks and the answer is over a minute old.
#define LOCKER_MAX 600
struct DayStat { uint16_t jpg = 0, avi = 0, wav = 0, other = 0; uint64_t bytes = 0; uint16_t hours[24] = {}; std::map<String, uint16_t> src; };

static SemaphoreHandle_t libLock = xSemaphoreCreateMutex();
static String libFiles, libStats;  // ready JSON answers ("" = not counted yet)
static int libTotal = 0;
static uint32_t libBuiltAt = 0;
static volatile bool libWanted = true;  // count once after start

static void libAsk(bool now = false) {
  if (now || !libBuiltAt || millis() - libBuiltAt > 60000UL) libWanted = true;
}

static void libBuild() {
  static Entry *list = (Entry *)ps_malloc(LOCKER_MAX * sizeof(Entry));
  if (!list) return;
  // min-heap of indexes into list: the oldest of the newest 600 sits on top
  std::vector<uint16_t> heap;
  heap.reserve(LOCKER_MAX);
  auto newer = [&](uint16_t a, uint16_t b) { return strcmp(list[a].name, list[b].name) > 0; };
  std::map<String, DayStat> days;
  uint32_t total = 0;
  uint64_t bytes = 0;
  lockSD();
  listSec([&](const char *, const char *name, uint32_t size) {
    total++;
    bytes += size;
    if (heap.size() < LOCKER_MAX) {
      uint16_t i = heap.size();
      strlcpy(list[i].name, name, sizeof(list[i].name)); list[i].size = size;
      heap.push_back(i); std::push_heap(heap.begin(), heap.end(), newer);
    } else if (strcmp(name, list[heap.front()].name) > 0) {
      std::pop_heap(heap.begin(), heap.end(), newer);
      uint16_t i = heap.back();
      strlcpy(list[i].name, name, sizeof(list[i].name)); list[i].size = size;
      std::push_heap(heap.begin(), heap.end(), newer);
    }
    char key[9];
    int sec = 0;
    const char *tag = nullptr;
    if (!evidenceDay(name, key, &sec, &tag)) return true;  // the case log and other odd names
    DayStat &d = days[String(key)];
    const char *ext = strrchr(name, '.');
    if (ext && !strcmp(ext, ".jpg")) d.jpg++;
    else if (ext && !strcmp(ext, ".avi")) d.avi++;
    else if (ext && !strcmp(ext, ".wav")) d.wav++;
    else d.other++;
    d.bytes += size;
    int h = sec / 3600;
    if (h < 24) d.hours[h]++;
    const char *end = strpbrk(tag, "-_.");
    d.src[String(tag).substring(0, end ? end - tag : strlen(tag))]++;
    return true;
  }, true);
  unlockSD();

  String f;
  f.reserve(heap.size() * 40 + 16);
  f = "[";
  for (size_t k = 0; k < heap.size(); k++) {
    if (k) f += ',';
    f += "[\""; f += list[heap[k]].name; f += "\","; f += list[heap[k]].size; f += ']';
  }
  f += "]";

  JsonDocument doc;
  doc["total"] = total;
  doc["mb"] = (uint32_t)(bytes / 1048576);
  doc["freeMB"] = live.freeMB;
  JsonArray a = doc["days"].to<JsonArray>();
  for (auto &kv : days) {
    DayStat &d = kv.second;
    JsonObject o = a.add<JsonObject>();
    o["d"] = kv.first; o["jpg"] = d.jpg; o["avi"] = d.avi; o["wav"] = d.wav; o["o"] = d.other;
    o["mb"] = (float)(d.bytes / 1048576.0);
    JsonArray h = o["h"].to<JsonArray>();
    for (int i = 0; i < 24; i++) h.add(d.hours[i]);
    JsonObject so = o["s"].to<JsonObject>();
    for (auto &x : d.src) so[x.first] = x.second;
  }
  String st;
  serializeJson(doc, st);

  xSemaphoreTake(libLock, portMAX_DELAY);
  libFiles = f; libStats = st; libTotal = total; libBuiltAt = millis();
  xSemaphoreGive(libLock);
}

static void libTask(void *) {
  for (;;) {
    delay(300);
    if (!libWanted || !live.sdOK) continue;
    libWanted = false;
    libBuild();
  }
}

static void sendReady(const String &which, bool files) {
  xSemaphoreTake(libLock, portMAX_DELAY);
  String s = which;
  int total = libTotal;
  xSemaphoreGive(libLock);
  if (!s.length()) {
    libAsk(true);
    server.sendHeader("Retry-After", "2");
    server.send(503, "text/plain", "Watson is still counting the evidence");
    return;
  }
  libAsk();
  if (files) server.sendHeader("X-Total-Files", String(total));
  sendJson(s);
}

// /api/files: the newest 600 files for the locker, or ?day=YYYYMMDD: up to MAX_LIST of that day (for the timeline)
static void handleFiles() {
  String day = server.arg("day");
  if (day.length() != 8) { sendReady(libFiles, true); return; }
  static Entry *list = (Entry *)ps_malloc(MAX_LIST * sizeof(Entry));
  if (!list) { server.send(500, "text/plain", "No memory"); return; }
  int n = 0, total = 0;
  lockSD();
  listDay(day, [&](const char *name, uint32_t size) {
    total++;
    if (n < MAX_LIST) { strlcpy(list[n].name, name, sizeof(list[n].name)); list[n++].size = size; }
  });
  unlockSD();
  sendEntries(list, n, total);
}

// /api/stats: per day the number of photos, videos and recordings, their size, clues per hour and per sensor
static void handleStats() { sendReady(libStats, false); }

static bool safeName(const String &n) { return n.length() && n.length() < 64 && n.indexOf('/') < 0 && n.indexOf("..") < 0; }

static void handleFile() {
  String n = server.arg("name");
  if (!safeName(n)) { server.send(400, "text/plain", "Bad name"); return; }
  lockSD();
  File f = openSD(findEvidence(n), FILE_READ);
  size_t size = f ? f.size() : 0;
  unlockSD();
  if (!f) { server.send(404, "text/plain", "No such clue"); return; }
  String type = n.endsWith(".jpg") ? "image/jpeg" : n.endsWith(".wav") ? "audio/wav" : n.endsWith(".avi") ? "video/x-msvideo"
              : n.endsWith(".csv") ? "text/plain" : "application/octet-stream";

  // Range requests (Safari needs them to play audio and seek)
  size_t from = 0, to = size ? size - 1 : 0;
  bool partial = false;
  String range = server.header("Range");
  if (range.startsWith("bytes=") && size) {
    int dash = range.indexOf('-');
    String a = range.substring(6, dash), b = range.substring(dash + 1);
    if (a.length()) { from = a.toInt(); if (b.length()) to = min((size_t)b.toInt(), size - 1); }
    else if (b.length()) { from = size - min((size_t)b.toInt(), size); }
    if (from > to) { f.close(); server.sendHeader("Content-Range", "bytes */" + String(size)); server.send(416, "text/plain", ""); return; }
    partial = true;
  }
  if (server.hasArg("dl")) server.sendHeader("Content-Disposition", "attachment; filename=" + n);
  server.sendHeader("Accept-Ranges", "bytes");
  if (partial) server.sendHeader("Content-Range", "bytes " + String(from) + "-" + String(to) + "/" + String(size));
  server.setContentLength(size ? to - from + 1 : 0);
  server.send(partial ? 206 : 200, type, "");

  // Send in pieces, holding the SD card only while reading, so a slow or paused phone
  // can't block recording; give up if the phone stops reading.
  NetworkClient client = server.client();
  client.setTimeout(3000);  // milliseconds
  static uint8_t buf[4096];
  size_t pos = from, left = size ? to - from + 1 : 0;
  while (left && client.connected()) {
    webAlive();
    lockSD();
    f.seek(pos);
    size_t n = f.read(buf, min(left, sizeof(buf)));
    unlockSD();
    if (!n || client.write(buf, n) != n) break;
    pos += n;
    left -= n;
  }
  lockSD();
  f.close();
  unlockSD();
}

static void handleDelete() {
  String n = server.arg("name");
  if (!safeName(n)) { server.send(400, "text/plain", "Bad name"); return; }
  lockSD();
  bool ok = SD.remove(findEvidence(n));
  unlockSD();
  if (ok) {
    logEvent("deleted", "web", n);
    xSemaphoreTake(libLock, portMAX_DELAY);
    int at = libFiles.indexOf("[\"" + n + "\",");
    if (at >= 0) {
      int end = libFiles.indexOf(']', at) + 1;
      if (end < (int)libFiles.length() && libFiles[end] == ',') end++;
      else if (at > 1 && libFiles[at - 1] == ',') at--;
      libFiles.remove(at, end - at);
      libTotal--;
    }
    xSemaphoreGive(libLock);
    libAsk(true);
  }
  server.send(ok ? 200 : 404, "text/plain", ok ? "Destroyed the evidence" : "No such clue");
}

// Delete one day's evidence (day "all" = everything), in batches so thousands of files need little
// memory, and one file at a time so recording can carry on. Returns the number of files deleted.
int deleteDay(const String &day, bool keepLog) {
  bool all = day == "all";
  int n = 0;
  std::vector<String> dirs;  // day folders to remove once emptied
  for (int round = 0; round < 1000; round++) {  // in batches, so thousands of files need little memory
    std::vector<String> names;  // full paths
    lockSD();
    listSec([&](const char *folder, const char *name, uint32_t) {
      if (keepLog && !strcmp(name, "caselog.csv")) return true;
      char k[9];
      if (!all && (!evidenceDay(name, k) || day != k)) return true;
      if (*folder && std::find(dirs.begin(), dirs.end(), folder) == dirs.end()) dirs.push_back(folder);
      names.push_back(String(SEC_DIR "/") + (*folder ? String(folder) + "/" : "") + name);
      return names.size() < 100;
    });
    unlockSD();
    if (names.empty()) break;
    int before = n;
    for (auto &name : names) {
      lockSD();  // one file at a time so recording can carry on
      if (SD.remove(name)) n++;
      webAlive();
      unlockSD();
    }
    if (n == before) break;  // nothing could be deleted, avoid looping forever
  }
  // remove the emptied day folders (rmdir refuses folders that still hold files)
  lockSD();
  for (auto &dname : dirs) SD.rmdir(String(SEC_DIR "/") + dname);
  unlockSD();
  return n;
}

// The oldest day that has evidence, other than today ("" if there is none)
String oldestDay() {
  String today = todayKey();
  char best[9] = "";
  lockSD();
  listSec([&](const char *, const char *name, uint32_t) {
    char k[9];
    if (!evidenceDay(name, k)) return true;
    if ((!best[0] || strcmp(k, best) < 0) && today != k) strlcpy(best, k, 9);
    return true;
  });
  unlockSD();
  return best;
}

// /api/deletebulk?day=YYYYMMDD  or  ?day=all  (&keeplog=0 to also delete the case log)
static void handleDeleteBulk() {
  String day = server.arg("day");
  bool all = day == "all";
  if (!all && (day.length() != 8 || day.toInt() < 20000101)) { server.send(400, "text/plain", "Bad day"); return; }
  int n = deleteDay(day, server.arg("keeplog") != "0");
  logEvent("deleted", "web", String(n) + " file(s) " + (all ? "- everything" : "from " + day));
  xSemaphoreTake(libLock, portMAX_DELAY);
  libFiles = ""; libStats = "";  // the page waits for a fresh count
  xSemaphoreGive(libLock);
  libAsk(true);
  server.send(200, "text/plain", String(n));
}

static void handleUpdateDone() {
  bool ok = !Update.hasError();
  server.send(200, "text/plain", ok ? "Update OK - restarting" : "Update FAILED");
  if (ok) { delay(500); ESP.restart(); }
}

static void handleUpdateUpload() {
  if (!auth()) return;
  HTTPUpload &u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    logEvent("update", "web", "firmware upload " + u.filename);
    Update.begin(UPDATE_SIZE_UNKNOWN);
  } else if (u.status == UPLOAD_FILE_WRITE) {
    // a wrong file (e.g. the 8 MB full USB image): stop writing at the first error instead of chewing through it
    if (!Update.hasError() && Update.write(u.buf, u.currentSize) != u.currentSize) {
      logEvent("error", "web", String("firmware upload refused: ") + Update.errorString());
      Update.abort();
    }
    webAlive();
  } else if (u.status == UPLOAD_FILE_END) {
    if (!Update.hasError()) Update.end(true);
  }
}

void webStart() {
  server.on("/", HTTP_GET, [] { webWhere = "/"; if (auth()) server.send_P(200, "text/html", INDEX_HTML); });
  ROUTE("/api/status", HTTP_GET, handleStatus());
  ROUTE("/api/events", HTTP_GET, sendJson(eventsJson(40)));
  ROUTE("/api/schema", HTTP_GET, sendJson(schemaJson()));
  ROUTE("/api/config", HTTP_GET, sendJson(configJson(true)));
  ROUTE("/api/config", HTTP_POST, handleSaveConfig());
  ROUTE("/api/arm", HTTP_POST, handleArm());
  ROUTE("/api/sleep", HTTP_POST, { if (!cfg.campEn) { server.send(400, "text/plain", "Camp mode is off."); return; } server.send(200, "text/plain", "Good night"); campSleepNow(); });
  ROUTE("/api/settime", HTTP_POST, { handleSetTime(); });
  ROUTE("/api/mesh/test", HTTP_POST, { if (!cfg.meshEn || cfg.meshTx < 0) { server.send(400, "text/plain", "Enable the Pigeon Post and set the TX pin first, then save."); return; } meshSend("221B test: Inspector Lestrade checks the pigeons. All in order." + battText(), true); server.send(200, "text/plain", "Sent - look at your Meshtastic app"); });
  ROUTE("/api/siren", HTTP_POST, { if (server.arg("on") == "1") sirenSound(cfg.sirenSec, "rung from the web page"); else sirenStop("silenced from the web page"); server.send(200, "text/plain", live.siren ? "1" : "0"); });
  ROUTE("/api/sirencfg", HTTP_POST, {  // quick bell switches on The Scene
    if (server.hasArg("en")) cfg.sirenEn = server.arg("en") == "1";
    if (server.hasArg("low")) cfg.sirenActiveLow = server.arg("low") == "1";
    if (!cfg.sirenEn) sirenStop("bell switched off");
    saveConfig();
    server.send(200, "text/plain", "ok");
  });
  ROUTE("/api/irregulars", HTTP_GET, sendJson(irregularsJson()));
  ROUTE("/api/irrmine", HTTP_POST, {  // "these are mine": the devices around now become the normal count
    irregularsMarkMine();
    cfg.irrBaseline = constrain(irregularsNow(), 0, 50);
    saveConfig();
    logEvent("irregulars", "web", "normal count set to " + String(cfg.irrBaseline) + " devices");
    server.send(200, "text/plain", String(cfg.irrBaseline));
  });
  ROUTE("/api/radarmm", HTTP_POST, {  // the trigger slider on The Scene: no pin restart, so the radar keeps tracking
    cfg.radarMaxMm = constrain(server.arg("mm").toInt(), 100, 6000);
    saveConfig();
    server.send(200, "text/plain", String(cfg.radarMaxMm));
  });
  ROUTE("/api/level", HTTP_POST, { shakeLevelNow(); logEvent("config", "web", "seismograph: resting angle taken now"); server.send(200, "text/plain", "ok"); });
  ROUTE("/api/test", HTTP_POST, { raiseEvent(SRC_TEST, "Inspector Lestrade", "manual test from the web page"); server.send(200, "text/plain", "ok"); });
  ROUTE("/api/tg/test", HTTP_POST, { if (!telegramReady()) { server.send(400, "text/plain", "Enable Telegram and fill in token and chat ID first, then save."); return; } telegramAlert("221B test: Inspector Lestrade checks the telegraph. All in order.", true, true); server.send(200, "text/plain", "Telegram sent - check your phone"); });
  ROUTE("/api/tg/findchat", HTTP_POST, server.send(200, "text/plain", telegramFindChat()));
  ROUTE("/api/motiongrid", HTTP_GET, sendJson(motionGridJson()));
  ROUTE("/api/files", HTTP_GET, handleFiles());
  ROUTE("/api/stats", HTTP_GET, handleStats());
  ROUTE("/api/delete", HTTP_POST, handleDelete());
  ROUTE("/api/deletebulk", HTTP_POST, handleDeleteBulk());
  ROUTE("/api/restart", HTTP_POST, { server.send(200, "text/plain", "ok"); delay(300); ESP.restart(); });
  ROUTE("/capture", HTTP_GET, handleCapture());
  ROUTE("/file", HTTP_GET, handleFile());
  server.on("/api/update", HTTP_POST, [] { if (auth()) handleUpdateDone(); }, handleUpdateUpload);
  static const char *headers[] = {"Range"};
  server.collectHeaders(headers, 1);
  server.begin();
  webAlive();
  xTaskCreatePinnedToCore(webWatchdog, "webdog", 4096, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(webTask, "web", 12288, nullptr, 2, nullptr, 0);
  xTaskCreatePinnedToCore(libTask, "librarian", 6144, nullptr, 1, nullptr, 0);
}

// The web page runs in its own task on core 0. In the Arduino loop() (core 1, lowest priority) it starved
// whenever the camera, microphone and sensors on core 1 were busy recording, and the page timed out.
static void webTask(void *) {
  uint32_t lastFlush = 0;
  for (;;) {
    webLoop();
    if (millis() - lastFlush > 1000) { lastFlush = millis(); flushEvents(); }
    delay(2);
  }
}

// If the web loop has not come round for 45 s, say where it was stuck and restart, so the page always comes back
static void webWatchdog(void *) {
  for (;;) {
    delay(5000);
    if (webBeat && millis() - webBeat > 45000) {
      Serial.printf("WEB WATCHDOG: page stuck for %lu s in %s - restarting\n", (millis() - webBeat) / 1000, (const char *)webWhere);
      logEvent("error", "web", String("page stuck in ") + (const char *)webWhere + " - restarting");
      delay(500);
      ESP.restart();
    }
  }
}

void webLoop() {
  webAlive();
  webWhere = "idle";
  server.handleClient();
  if (webBusy()) lastRequest = millis();
}
