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
  static const char *n[] = {"sound", "motion", "input1", "input2", "input3", "input4", "radar", "sonar", "seismograph", "irregulars", "test"};
  return s < SRC_COUNT ? n[s] : "?";
}

Actions actionsFor(Source s) {
  switch (s) {
    case SRC_SOUND:  return cfg.soundAct;
    case SRC_MOTION: return cfg.motionAct;
    case SRC_RADAR:  return cfg.radarAct;
    case SRC_SONAR:  return cfg.sonarAct;
    case SRC_SHAKE:  return cfg.shakeAct;
    case SRC_IRREG:  return cfg.irrAct;
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
  add(g, k + "_notify", "Send an alert (Telegram and LoRa)", 'b', &a.notify);
  add(g, k + "_siren", "Ring the alarm bell", 'b', &a.siren);
}

static const char *PULL_OPTS = "0:No pull (external resistor)|1:Pull-up (switch to GND)|2:Pull-down (switch to 3V3)";
static const char *EDGE_OPTS = "1:goes HIGH|0:goes LOW|2:either way";

void buildFields() {
  // defaults that depend on enums
  cfg.frameSize = FRAMESIZE_SVGA;
  for (int i = 0; i < MAX_INPUTS; i++) {
    cfg.in[i] = {false, "", GPIO_PINS[i], 1, 1, {true, true, true, true}};
    snprintf(cfg.in[i].name, sizeof(cfg.in[i].name), i == 0 ? "Front door" : "Input %d", i + 1);
  }
  cfg.in[1].pull = 0;  // second input defaults to a PIR (drives its own output)
  strlcpy(cfg.in[1].name, "PIR", sizeof(cfg.in[1].name));
  cfg.in[3].pull = 2;  // fourth input defaults to an RCWL-0516 microwave sensor (OUT drives 3.3 V)
  strlcpy(cfg.in[3].name, "RCWL-0516", sizeof(cfg.in[3].name));

  fields.clear();
  String g = "Baker Street HQ";
  add(g, "armed", "Armed (the game is afoot)", 'b', &cfg.armed);
  add(g, "cooldownSec", "Cooldown between events of the same kind (s)", 'i', &cfg.cooldownSec, 0, 3600, nullptr,
      "Stops one door slam from filling the card with 400 photos.");
  add(g, "debounceMs", "Tripwire debounce (ms)", 'i', &cfg.debounceMs, 0, 1000, nullptr,
      "How long a pin must stay at its new level before it counts. Lower reacts faster (pins are read every 5 ms); raise it if a bouncy switch fires twice.");
  add(g, "autoClean", "Make room: delete the oldest day when the card is full", 'b', &cfg.autoClean);
  add(g, "cleanPct", "Card counts as full above (%)", 'i', &cfg.cleanPct, 50, 99, nullptr,
      "Whole days go, oldest first, until the card is below this again. Today and the case log are never touched.");
  add(g, "tz", "Time zone (POSIX TZ string)", 's', cfg.tz, 0, sizeof(cfg.tz), nullptr,
      "Pacific: PST8PDT,M3.2.0,M11.1.0  Central Europe: CET-1CEST,M3.5.0,M10.5.0/3  UTC: UTC0");

  g = "The Second Witness (fewer false alarms)";
  add(g, "witnessEn", "Only alarm when two different sensors agree", 'b', &cfg.witnessEn, 0, 0, nullptr,
      "One sensor alone is only noted in the case log. When a second, different sensor fires soon after (PIR then radar, radar then microwave...), the alarm goes off with that sensor's actions. A cat or a gust rarely fools two kinds of sensor.");
  add(g, "witnessSec", "Second sensor must fire within (s)", 'i', &cfg.witnessSec, 1, 60);

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
  add(g, "tgCmdEn", "Two-way: accept commands from your chat", 'b', &cfg.tgCmdEn, 0, 0, nullptr,
      "Send the bot status, arm (arm 2h), disarm (disarm 30), mute 1h, unmute, photo, last, audio 5, video 5, log, ring, quiet, sleep or help (with or without /), or tap the buttons. Only your own chat ID is obeyed, so no PIN is needed. "
      "Photo, audio and video are saved to the card and sent to you. Keeps one connection to Telegram open (about 40 KB of memory).");

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
  add(g, "radarTx", "ESP TX pin (to radar RX, optional)", 'p', &cfg.radarTx);
  add(g, "radarMaxMm", "Trigger when someone is closer than (mm)", 'i', &cfg.radarMaxMm, 100, 6000);
  addActions(g, "radar", cfg.radarAct);

  g = "The Bat Ears (HC-SR04 ultrasonic)";
  add(g, "sonarEn", "Ultrasonic enabled", 'b', &cfg.sonarEn);
  add(g, "sonarTrig", "TRIG pin", 'p', &cfg.sonarTrig);
  add(g, "sonarEcho", "ECHO pin (via 5V->3.3V divider!)", 'p', &cfg.sonarEcho);
  add(g, "sonarCm", "Trigger when something is closer than (cm)", 'i', &cfg.sonarCm, 5, 400);
  addActions(g, "sonar", cfg.sonarAct);

  g = "The Seismograph (MPU6050 tilt and shock)";
  add(g, "shakeEn", "Seismograph enabled", 'b', &cfg.shakeEn, 0, 0, nullptr,
      "A GY-521 / MPU6050 board: VCC to 3V3, GND, SDA and SCL to the pins below. Screw or glue it firmly to the car body, tent pole or box: "
      "a loose board rattles. Tilt catches a car being jacked up (wheel or catalytic converter theft) or towed; shock catches a smashed window, a forced door or a kick. "
      "The resting angle is taken when you arm (and with Level now on The Scene); very slow changes (heat, tyres cooling) are followed by themselves.");
  add(g, "shakeSda", "SDA pin", 'p', &cfg.shakeSda);
  add(g, "shakeScl", "SCL pin", 'p', &cfg.shakeScl);
  add(g, "shakeTiltDeg", "Alarm when tilted more than (degrees, 0 = off)", 'i', &cfg.shakeTiltDeg, 0, 45, nullptr,
      "Car: 2. A jack lifting one wheel tilts a car about 3 to 5 degrees.");
  add(g, "shakeShockMg", "Alarm on a jolt stronger than (milli-g, 0 = off)", 'i', &cfg.shakeShockMg, 0, 4000, nullptr,
      "Car: 80 to 150 (lower = more sensitive). A slammed door nearby or a passing truck can reach 50. Watch the live value on The Scene while you knock. "
      "With Two witnesses on, a jolt plus a loud sound within the witness time is a strong break-in sign.");
  addActions(g, "shake", cfg.shakeAct);

  g = "The Baker Street Irregulars (phones and watches nearby)";
  add(g, "irrEn", "Listen for phones nearby (Wi-Fi)", 'b', &cfg.irrEn, 0, 0, nullptr,
      "Counts phones around the house from the Wi-Fi probe requests they send while looking for known networks. Out in the wild there are none, so new ones mean someone is near. People without a phone (or with Wi-Fi off) are invisible to it; animals never trigger it. Needs Wi-Fi or the hotspot on. Only counts are kept, nothing leaves the board.");
  add(g, "irrRssi", "Only count devices stronger than (dBm)", 'i', &cfg.irrRssi, -100, -30, nullptr,
      "-50 is a few metres away, -80 tens of metres. The Scene shows the strongest devices so you can choose.");
  add(g, "irrBaseline", "Devices normally around (yours)", 'i', &cfg.irrBaseline, 0, 50, nullptr,
      "Press These are mine on The Scene to set it to the current count.");
  add(g, "irrExtra", "Alarm when this many more appear", 'i', &cfg.irrExtra, 1, 10);
  add(g, "irrHoldSec", "...and stay for at least (s)", 'i', &cfg.irrHoldSec, 5, 120, nullptr,
      "Phones change their Wi-Fi address now and then; waiting a little stops that from looking like a visitor.");
  addActions(g, "irr", cfg.irrAct);

  g = "The Night Watch (IR lamp & black and white)";
  add(g, "nightMode", "Night watch (on or off)", 'e', &cfg.nightMode, 0, 0, "0:Off|1:When dark|2:By the clock|3:Always on",
      "The light level is shown on The Scene next to the Night Watch lamp. If your IR lamp lights the room so well that it looks like day, use By the clock.");
  add(g, "nightBelow", "Night begins below light level", 'i', &cfg.nightBelow, 0, 255);
  add(g, "nightAbove", "Day returns above light level", 'i', &cfg.nightAbove, 0, 255, nullptr,
      "Keep this well above the night level so the lamp does not flicker on and off.");
  add(g, "nightFrom", "By the clock: night from (HHMM)", 'i', &cfg.nightFrom, 0, 2359);
  add(g, "nightTo", "By the clock: night until (HHMM)", 'i', &cfg.nightTo, 0, 2359);
  add(g, "nightBW", "Black and white picture at night (less colour noise)", 'b', &cfg.nightBW);
  add(g, "irPin", "IR lamp output pin (HIGH = lamp on)", 'p', &cfg.irPin, 0, 0, nullptr,
      "Drive the IR LEDs through a transistor or MOSFET, never straight from the pin. The OV3660 sees IR only if the lens has no IR-cut filter.");

  g = "The Alarm Bell (siren, buzzer or LED)";
  add(g, "sirenEn", "Alarm bell on", 'b', &cfg.sirenEn);
  add(g, "sirenPin", "Output pin", 'p', &cfg.sirenPin, 0, 0, nullptr,
      "A buzzer or LED can hang on the pin with a resistor; a real siren needs a transistor, MOSFET or relay module. Tick Ring the alarm bell under each sensor that should ring it.");
  add(g, "sirenActiveLow", "Rings when the pin is", 'b', &cfg.sirenActiveLow, 0, 0,
      "0:Active HIGH|1:Active LOW",
      "HIGH for an LED, buzzer or transistor; LOW for most relay modules. Also switchable on The Scene.");
  add(g, "sirenSec", "Ring for (s)", 'i', &cfg.sirenSec, 1, 300);
  add(g, "sirenStyle", "Sound", 'e', &cfg.sirenStyle, 0, 0, "0:Steady|1:Slow beep|2:Fast blink|3:Piezo wail (passive piezo)|4:Piezo beeps (passive piezo)");

  g = "The Diogenes Club (MQTT & Home Assistant)";
  add(g, "mqttEn", "Report to an MQTT broker", 'b', &cfg.mqttEn);
  add(g, "mqttHost", "Broker address (IP or name)", 's', cfg.mqttHost, 0, sizeof(cfg.mqttHost), nullptr,
      "Home Assistant: install the Mosquitto broker add-on and the MQTT integration, then enter the Home Assistant IP here.");
  add(g, "mqttPort", "Port", 'i', &cfg.mqttPort, 1, 65535);
  add(g, "mqttUser", "User name", 's', cfg.mqttUser, 0, sizeof(cfg.mqttUser));
  add(g, "mqttPass", "Password", 'w', cfg.mqttPass, 0, sizeof(cfg.mqttPass));
  add(g, "mqttBase", "Topic prefix", 's', cfg.mqttBase, 0, sizeof(cfg.mqttBase), nullptr,
      "Publishes PREFIX/state, PREFIX/event and PREFIX/clue. Listens on PREFIX/set/armed, PREFIX/set/siren (ON or OFF) and PREFIX/set/test.");
  add(g, "mqttHa", "Announce to Home Assistant (auto discovery)", 'b', &cfg.mqttHa);

  g = "The Pigeon Post (Meshtastic LoRa)";
  add(g, "meshEn", "Send alerts over a Meshtastic radio", 'b', &cfg.meshEn, 0, 0, nullptr,
      "On the radio (Meshtastic app > Module config > Serial): enable, mode TEXTMSG, baud 38400, and set its RX/TX pins. Wire ESP TX to radio RX, ESP RX to radio TX, and GND to GND. Every alert becomes a text on the primary channel.");
  add(g, "meshTx", "ESP TX pin (to radio RX)", 'p', &cfg.meshTx);
  add(g, "meshRx", "ESP RX pin (from radio TX, optional)", 'p', &cfg.meshRx);
  add(g, "meshBaud", "Baud rate", 'e', &cfg.meshBaud, 0, 0, "9600:9600|19200:19200|38400:38400|57600:57600|115200:115200");
  add(g, "meshGapSec", "At most one LoRa text every (s)", 'i', &cfg.meshGapSec, 5, 3600, nullptr,
      "LoRa is slow and shared. One minute keeps the channel friendly.");
  add(g, "meshCmdEn", "Two-way: accept commands over LoRa", 'b', &cfg.meshCmdEn, 0, 0, nullptr,
      "Needs the ESP RX pin wired to the radio's TX. Text the channel just the word: status, arm (arm 2h), disarm (disarm 30), mute 1h, unmute, log, ring, quiet, photo, audio, video, sleep or help "
      "(with a PIN set: PIN status). The camera answers right away. "
      "Use a private Meshtastic channel with your own key: anyone on the default channel could read these texts.");
  add(g, "meshPin", "Command PIN (optional)", 'w', cfg.meshPin, 0, sizeof(cfg.meshPin), nullptr,
      "Empty = no PIN: anyone on your private channel can send commands. With a PIN, write it before the command (1234 status); wrong PINs are logged and 5 lock commands for 10 minutes.");

  g = "The Camp (battery, sleep and hotspot)";
  add(g, "campEn", "Camp mode: sleep deeply between visitors", 'b', &cfg.campEn, 0, 0, nullptr,
      "While asleep there is no web page and no Wi-Fi. Movement on the wake pin wakes the house: it records with no Wi-Fi, sends a LoRa alert and sleeps again. Press BOOT to wake it with the hotspot Baker-Street-221B (or your Wi-Fi if it is in range) to look at the evidence or switch camp mode off.");
  add(g, "campPin", "Wake pin (the PIR, D0-D5 only)", 'p', &cfg.campPin, 0, 0, nullptr,
      "Use the same pin as your PIR tripwire so its photo, video and alert settings apply. D6 and D7 cannot wake the chip.");
  add(g, "campLevel", "Wake when the pin", 'e', &cfg.campLevel, 0, 0, "1:goes HIGH (PIR)|0:goes LOW (switch to GND)");
  add(g, "campAwakeSec", "Stay awake after the last movement (s)", 'i', &cfg.campAwakeSec, 5, 600);
  add(g, "campHotspotMin", "After BOOT or power on, stay awake without visitors (min)", 'i', &cfg.campHotspotMin, 1, 120);
  add(g, "campBeatMin", "Heartbeat over LoRa every (min, 0 = never)", 'i', &cfg.campBeatMin, 0, 1440, nullptr,
      "Wakes briefly, sends battery and card status, sleeps again. 360 = four times a day.");
  add(g, "battPin", "Battery voltage pin (through a divider)", 'p', &cfg.battPin, 0, 0, nullptr,
      "Two equal resistors (e.g. 2 x 100k) from battery + to GND, middle to the pin. Never put more than 3.3 V on the pin.");
  add(g, "battMul", "Divider ratio x100 (two equal resistors = 200)", 'i', &cfg.battMul, 100, 1000);
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

static SemaphoreHandle_t prefsLock = xSemaphoreCreateMutex();  // web, LoRa, timetable and MQTT can all save at once

// Settings live in NVS as one blob ("cfgb"). Older firmware kept them as one string ("cfg"): a ~3 KB string needs
// one NVS page with room for all of it, so once the pages were fragmented the save failed silently and the board
// came back with old settings after the next restart. A blob is split across pages, and every save is checked.
void loadConfig() {
  xSemaphoreTake(prefsLock, portMAX_DELAY);
  prefs.begin("baker221b", true);
  String s;
  size_t n = prefs.isKey("cfgb") ? prefs.getBytesLength("cfgb") : 0;
  if (n) {
    char *buf = (char *)malloc(n + 1);
    if (buf) { prefs.getBytes("cfgb", buf, n); buf[n] = 0; s = buf; free(buf); }
  }
  if (!s.length()) s = prefs.getString("cfg", "");
  live.nvsFree = prefs.freeEntries();
  prefs.end();
  xSemaphoreGive(prefsLock);
  String err;
  if (s.length()) applyConfigJson(s, err);
}

void saveConfig() {
  String s = configJson(false);
  xSemaphoreTake(prefsLock, portMAX_DELAY);
  prefs.begin("baker221b", false);
  bool ok = prefs.putBytes("cfgb", s.c_str(), s.length()) == s.length();
  if (!ok) {  // no room for the new copy next to the old one: drop the old one and try once more
    prefs.remove("cfgb");
    ok = prefs.putBytes("cfgb", s.c_str(), s.length()) == s.length();
  }
  if (ok && prefs.isKey("cfg")) prefs.remove("cfg");  // the old string copy is no longer needed
  live.nvsFree = prefs.freeEntries();
  prefs.end();
  xSemaphoreGive(prefsLock);
  live.saveFailed = !ok;
  if (!ok) logEvent("warning", "settings", "could not save the settings to flash (" + String(s.length()) + " bytes, " +
                    String(live.nvsFree) + " free entries): they are lost at the next restart");
}
