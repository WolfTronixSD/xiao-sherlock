// Configuration: one table describes every setting. It drives saving/loading (JSON in flash)
// and the web GUI forms, so adding an option is one line in buildFields().
#include "app.h"
#include "esp_camera.h"
#include <ArduinoJson.h>
#include <Preferences.h>

Config cfg;
LiveState live;
std::vector<Field> fields;

// Header pins that are free for sensors (D8-D10 belong to the SD card)
const int  GPIO_PINS[]  = {1, 2, 3, 4, 5, 6, 43, 44};
const char *GPIO_NAMES[] = {"D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7"};
const int  NUM_GPIO    = 8;

String pinName(int pin) {
  for (int i = 0; i < NUM_GPIO; i++)
    if (GPIO_PINS[i] == pin) return String(GPIO_NAMES[i]) + " (GPIO" + pin + ")";
  return pin < 0 ? "none" : "GPIO" + String(pin);
}

const char *sourceName(Source s) {
  static const char *n[] = {"sound", "motion", "input1", "input2", "input3", "input4", "radar", "sonar", "test"};
  return s < SRC_COUNT ? n[s] : "?";
}

Actions actionsFor(Source s) {
  switch (s) {
    case SRC_SOUND:  return cfg.soundAct;
    case SRC_MOTION: return cfg.motionAct;
    case SRC_RADAR:  return cfg.radarAct;
    case SRC_SONAR:  return cfg.sonarAct;
    case SRC_TEST:   return {true, true, true, true};
    default:         return cfg.in[s - SRC_INPUT1].act;
  }
}

// Fields with String keys/labels need stable storage for generated names (inputs 1-4)
static void add(const String &g, const String &k, const String &l, char t, void *p, int mn = 0, int mx = 0,
                const char *o = nullptr, const char *h = nullptr) {
  fields.push_back({g, k, l, t, p, mn, mx, o, h});
}
static void addActions(const String &g, const String &k, Actions &a) {
  add(g, k + "_photo", "Take photos", 'b', &a.photo);
  add(g, k + "_video", "Record video clip", 'b', &a.video);
  add(g, k + "_audio", "Record audio clip", 'b', &a.audio);
  add(g, k + "_notify", "Send Telegram alert", 'b', &a.notify);
}

static const char *PULL_OPTS = "0:No pull (external resistor)|1:Pull-up (switch to GND)|2:Pull-down (switch to 3V3)";
static const char *EDGE_OPTS = "1:goes HIGH|0:goes LOW|2:changes either way";

void buildFields() {
  // defaults that depend on enums
  cfg.frameSize = FRAMESIZE_SVGA;
  for (int i = 0; i < MAX_INPUTS; i++) {
    cfg.in[i] = {false, "", GPIO_PINS[i], 1, 1, {true, true, true, true}};
    snprintf(cfg.in[i].name, sizeof(cfg.in[i].name), i == 0 ? "Front door" : "Input %d", i + 1);
  }
  cfg.in[1].pull = 0;  // second input defaults to a PIR (drives its own output)
  strlcpy(cfg.in[1].name, "PIR", sizeof(cfg.in[1].name));

  fields.clear();
  String g = "Baker Street HQ";
  add(g, "armed", "Armed (the game is afoot)", 'b', &cfg.armed);
  add(g, "cooldownSec", "Cooldown between events of the same kind (s)", 'i', &cfg.cooldownSec, 0, 3600, nullptr,
      "Stops one door slam from filling the card with 400 photos.");
  add(g, "tz", "Time zone (POSIX TZ string)", 's', cfg.tz, 0, sizeof(cfg.tz), nullptr,
      "Pacific: PST8PDT,M3.2.0,M11.1.0  Central Europe: CET-1CEST,M3.5.0,M10.5.0/3  UTC: UTC0");

  g = "Mrs Hudson's Timetable (schedule & phone)";
  add(g, "schedEn", "Arm and disarm on a daily timetable", 'b', &cfg.schedEn);
  add(g, "schedArm", "Arm at (HHMM, 2300 = 11 pm)", 'i', &cfg.schedArm, 0, 2359);
  add(g, "schedDisarm", "Disarm at (HHMM, 0700 = 7 am)", 'i', &cfg.schedDisarm, 0, 2359, nullptr,
      "Acts only at that minute, so arming by hand still works. The morning disarm is skipped while your phone is out.");
  add(g, "homeEn", "Watch for my phone on the Wi-Fi", 'b', &cfg.homeEn);
  add(g, "homeIp", "Phone IP address", 's', cfg.homeIp, 0, sizeof(cfg.homeIp), nullptr,
      "Give the phone a fixed address in your router (and on iPhone turn off Private Wi-Fi Address for this network). Arms when the phone leaves, disarms when it comes back.");
  add(g, "homeAwayMin", "Call it 'out' after no answer for (min)", 'i', &cfg.homeAwayMin, 2, 120, nullptr,
      "Phones doze on Wi-Fi; 15 minutes avoids false alarms.");

  g = "Telegraph Office (Wi-Fi & access)";
  add(g, "wifiSsid", "Wi-Fi name (2.4 GHz)", 's', cfg.wifiSsid, 0, sizeof(cfg.wifiSsid));
  add(g, "wifiPass", "Wi-Fi password", 'w', cfg.wifiPass, 0, sizeof(cfg.wifiPass));
  add(g, "hostname", "Network name (http://NAME.local)", 's', cfg.hostname, 0, sizeof(cfg.hostname));
  add(g, "webPass", "Web page password (user: holmes, empty = none)", 'w', cfg.webPass, 0, sizeof(cfg.webPass));

  g = "The Telegram Boy (phone alerts)";
  add(g, "tgEn", "Send Telegram alerts", 'b', &cfg.tgEn);
  add(g, "tgToken", "Bot token (from @BotFather)", 'w', cfg.tgToken, 0, sizeof(cfg.tgToken), nullptr,
      "In Telegram: open @BotFather, send /newbot, follow the steps and paste the token here. Save, send your bot any message, then press Find my chat ID below.");
  add(g, "tgChat", "Chat ID", 's', cfg.tgChat, 0, sizeof(cfg.tgChat));
  add(g, "tgPhoto", "Attach a photo", 'b', &cfg.tgPhoto);
  add(g, "tgGapSec", "At most one alert every (s)", 'i', &cfg.tgGapSec, 0, 3600);

  g = "The Camera Obscura";
  static String frameOpts = String(FRAMESIZE_QVGA) + ":320x240|" + FRAMESIZE_CIF + ":400x296|" + FRAMESIZE_VGA +
                            ":640x480|" + FRAMESIZE_SVGA + ":800x600|" + FRAMESIZE_XGA + ":1024x768|" + FRAMESIZE_HD +
                            ":1280x720|" + FRAMESIZE_SXGA + ":1280x1024|" + FRAMESIZE_UXGA + ":1600x1200";
  add(g, "frameSize", "Resolution", 'e', &cfg.frameSize, 0, 0, frameOpts.c_str());
  add(g, "jpegQuality", "JPEG quality (6 best ... 40 smallest)", 'i', &cfg.jpegQuality, 6, 40);
  add(g, "vflip", "Flip picture vertically", 'b', &cfg.vflip);
  add(g, "hmirror", "Mirror picture", 'b', &cfg.hmirror);
  add(g, "photoCount", "Photos per event", 'i', &cfg.photoCount, 0, 10);
  add(g, "photoIntervalMs", "Time between photos (ms)", 'i', &cfg.photoIntervalMs, 100, 10000);
  add(g, "videoSec", "Video clip length (s)", 'i', &cfg.videoSec, 2, 120);
  add(g, "videoMaxSec", "Longest video when events keep coming (s)", 'i', &cfg.videoMaxSec, 5, 300);

  g = "The Listening Post (microphone)";
  add(g, "soundEn", "Listen for noises", 'b', &cfg.soundEn);
  add(g, "soundDb", "Loudness threshold (dB, -90 quiet ... 0 deafening)", 'i', &cfg.soundDb, -90, 0);
  add(g, "audioGain", "Recording volume boost (x)", 'i', &cfg.audioGain, 1, 32);
  add(g, "audioHoldSec", "Keep recording until quiet for (s)", 'i', &cfg.audioHoldSec, 1, 30);
  add(g, "audioMaxSec", "Longest audio clip (s)", 'i', &cfg.audioMaxSec, 2, 120);
  add(g, "audioTrigSec", "Audio length when another sensor asks (s)", 'i', &cfg.audioTrigSec, 1, 120);
  addActions(g, "sound", cfg.soundAct);

  g = "Watching Brief (motion zones)";
  add(g, "motionEn", "Detect motion", 'b', &cfg.motionEn);
  add(g, "motionPixel", "Pixel change needed (1 twitchy ... 100 blind)", 'i', &cfg.motionPixel, 1, 100);
  add(g, "motionLightPct", "Ignore if more than this % of the picture changes (lights)", 'i', &cfg.motionLightPct, 10, 100);
  addActions(g, "motion", cfg.motionAct);

  for (int i = 0; i < MAX_INPUTS; i++) {
    g = "Tripwire " + String(i + 1) + " (GPIO input)";
    String k = "in" + String(i + 1);
    add(g, k + "_en", "Enabled", 'b', &cfg.in[i].en);
    add(g, k + "_name", "Name", 's', cfg.in[i].name, 0, sizeof(cfg.in[i].name));
    add(g, k + "_pin", "Pin", 'p', &cfg.in[i].pin);
    add(g, k + "_pull", "Input mode", 'e', &cfg.in[i].pull, 0, 0, PULL_OPTS);
    add(g, k + "_edge", "Trigger when pin", 'e', &cfg.in[i].edge, 0, 0, EDGE_OPTS,
        i == 0 ? "Door reed switch: wire pin to GND, Pull-up, goes HIGH (door opens). PIR: No pull, goes HIGH." : nullptr);
    addActions(g, k, cfg.in[i].act);
  }

  g = "The Radar (HLK-LD2450)";
  add(g, "radarEn", "Radar enabled", 'b', &cfg.radarEn);
  add(g, "radarRx", "ESP RX pin (to radar TX)", 'p', &cfg.radarRx);
  add(g, "radarTx", "ESP TX pin (to radar RX)", 'p', &cfg.radarTx);
  add(g, "radarMaxMm", "Trigger when someone is closer than (mm)", 'i', &cfg.radarMaxMm, 100, 6000);
  addActions(g, "radar", cfg.radarAct);

  g = "The Bat Ears (HC-SR04 ultrasonic)";
  add(g, "sonarEn", "Ultrasonic enabled", 'b', &cfg.sonarEn);
  add(g, "sonarTrig", "TRIG pin", 'p', &cfg.sonarTrig);
  add(g, "sonarEcho", "ECHO pin (via 5V->3.3V divider!)", 'p', &cfg.sonarEcho);
  add(g, "sonarCm", "Trigger when something is closer than (cm)", 'i', &cfg.sonarCm, 5, 400);
  addActions(g, "sonar", cfg.sonarAct);
}

static void writeField(JsonObject o, const Field &f, bool forWeb) {
  switch (f.type) {
    case 'b': o[f.key] = *(bool *)f.ptr; break;
    case 'i': case 'p': case 'e': o[f.key] = *(int *)f.ptr; break;
    case 's': o[f.key] = (const char *)f.ptr; break;
    case 'w': o[f.key] = forWeb ? (*(char *)f.ptr ? "********" : "") : (const char *)f.ptr; break;
  }
}

String configJson(bool forWeb) {
  JsonDocument doc;
  JsonObject o = doc.to<JsonObject>();
  for (auto &f : fields) writeField(o, f, forWeb);
  JsonArray zs = o["zones"].to<JsonArray>();
  for (auto &z : cfg.zones) {
    JsonObject j = zs.add<JsonObject>();
    j["en"] = z.en; j["name"] = z.name; j["minPct"] = z.minPct;
    j["x"] = z.x; j["y"] = z.y; j["w"] = z.w; j["h"] = z.h;
  }
  String s;
  serializeJson(doc, s);
  return s;
}

static bool validPin(int p) {
  if (p == -1) return true;
  for (int i = 0; i < NUM_GPIO; i++) if (GPIO_PINS[i] == p) return true;
  return false;
}

bool applyConfigJson(const String &json, String &err) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) { err = "Bad JSON"; return false; }
  JsonObject o = doc.as<JsonObject>();
  for (auto &f : fields) {
    JsonVariant v = o[f.key];
    if (v.isNull()) continue;
    switch (f.type) {
      case 'b': *(bool *)f.ptr = v.as<bool>(); break;
      case 'i': *(int *)f.ptr = constrain(v.as<int>(), f.min, f.max); break;
      case 'e': *(int *)f.ptr = v.as<int>(); break;
      case 'p': if (validPin(v.as<int>())) *(int *)f.ptr = v.as<int>(); break;
      case 's': strlcpy((char *)f.ptr, v.as<const char *>() ? v.as<const char *>() : "", f.max); break;
      case 'w': {
        const char *s = v.as<const char *>();
        if (s && strcmp(s, "********")) strlcpy((char *)f.ptr, s, f.max);
        break;
      }
    }
  }
  if (o["zones"].is<JsonArray>()) {
    JsonArray zs = o["zones"];
    for (int i = 0; i < MAX_ZONES; i++) {
      Zone &z = cfg.zones[i];
      if (i >= (int)zs.size()) { z = Zone{}; continue; }
      JsonObject j = zs[i];
      z.en = j["en"] | false;
      strlcpy(z.name, j["name"] | "Zone", sizeof(z.name));
      z.minPct = constrain((int)(j["minPct"] | 5), 1, 100);
      z.x = constrain((float)(j["x"] | 0.0f), 0.0f, 1.0f);
      z.y = constrain((float)(j["y"] | 0.0f), 0.0f, 1.0f);
      z.w = constrain((float)(j["w"] | 1.0f), 0.0f, 1.0f - z.x);
      z.h = constrain((float)(j["h"] | 1.0f), 0.0f, 1.0f - z.y);
    }
  }
  return true;
}

String schemaJson() {
  JsonDocument doc;
  JsonArray a = doc.to<JsonArray>();
  for (auto &f : fields) {
    JsonObject o = a.add<JsonObject>();
    o["g"] = f.group; o["k"] = f.key; o["l"] = f.label; o["t"] = String(f.type);
    if (f.type == 'i') { o["min"] = f.min; o["max"] = f.max; }
    if (f.opts) o["o"] = f.opts;
    if (f.help) o["h"] = f.help;
  }
  JsonArray pins = doc.add<JsonObject>()["pins"].to<JsonArray>();
  for (int i = 0; i < NUM_GPIO; i++) {
    JsonArray p = pins.add<JsonArray>();
    p.add(GPIO_PINS[i]);
    p.add(pinName(GPIO_PINS[i]));
  }
  String s;
  serializeJson(doc, s);
  return s;
}

static Preferences prefs;

void loadConfig() {
  prefs.begin("baker221b", true);
  String s = prefs.getString("cfg", "");
  prefs.end();
  String err;
  if (s.length()) applyConfigJson(s, err);
}

void saveConfig() {
  prefs.begin("baker221b", false);
  prefs.putString("cfg", configJson(false));
  prefs.end();
}
