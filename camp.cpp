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

// ---------- two-way: commands over LoRa ("status", or "PIN status" when a PIN is set) ----------
// In TEXTMSG mode the radio prints every channel message it receives as one line, usually "Name: text".
static uint32_t snoozeUntil = 0;   // when "snooze N" re-arms by itself (0 = no snooze)
static bool snoozeTg = false;      // the snooze came from Telegram: say so there when it ends
static uint8_t badPins = 0;
static uint32_t lockedUntil = 0, lastBadLog = 0;

static const char *CMDS[] = {"arm", "disarm", "status", "ring", "quiet", "snooze", "photo", "audio", "video", "last", "log", "mute", "unmute", "sleep", "help"};
static uint32_t armUntil = 0;  // "arm 2h": disarms by itself then
static bool armTg = false;

// "2h", "90m", "45" (minutes) -> minutes; def when empty
static int durMin(const String &a, int def) {
  if (!a.length()) return def;
  int v = a.toInt();
  if (v <= 0) return def;
  char u = tolower(a[a.length() - 1]);
  return constrain(u == 'h' ? v * 60 : v, 1, 24 * 60);
}
static String durText(int m) { return m % 60 == 0 ? String(m / 60) + " h" : String(m) + " min"; }

static void reply(const String &t) { meshSend("221B: " + t, true); }  // replies skip the one-text-a-minute limit
static void loraReply(const String &t) { reply(t); }

// Shared by LoRa and Telegram: "via" is "LoRa" or "Telegram", out() sends the answer back the same way
static void runCommand(String cmd, const String &arg, const String &who, const char *via, void (*out)(const String &)) {
  auto reply = out;
  String by = String(via) + (who.length() ? " (" + who + ")" : String(""));
  logEvent(strcmp(via, "LoRa") ? "telegram" : "mesh", by, "command: " + cmd + (arg.length() ? " " + arg : ""));
  bool tg = strcmp(via, "Telegram") == 0;
  if (cmd == "disarm" && arg.length()) { cmd = "snooze"; }  // "disarm 30" = snooze 30
  if (cmd == "arm") {
    snoozeUntil = 0;
    setArmed(true, by, String("command over ") + via);
    if (arg.length()) {
      int m = durMin(arg, 60);
      armUntil = millis() + m * 60000UL; if (!armUntil) armUntil = 1;
      armTg = tg;
      reply("armed for " + durText(m) + ", then disarmed by itself.");
    } else { armUntil = 0; reply("armed. The game is afoot."); }
  }
  else if (cmd == "disarm") { snoozeUntil = 0; armUntil = 0; setArmed(false, by, String("command over ") + via); reply("disarmed. Holmes is off duty."); }
  else if (cmd == "snooze") {
    armUntil = 0;
    int m = durMin(arg, 30);
    if (m > 720) m = 720;
    setArmed(false, by, String("snoozed over ") + via + " for " + String(m) + " min");
    snoozeUntil = millis() + m * 60000UL;
    snoozeTg = strcmp(via, "Telegram") == 0;
    if (!snoozeUntil) snoozeUntil = 1;
    reply("disarmed for " + durText(m) + ", then armed again by itself.");
  } else if (cmd == "status") {
    int ph = irregularsNow();
    String t = String(cfg.armed ? "armed" : "disarmed");
    if (snoozeUntil) t += " (snoozed, " + String((int32_t)(snoozeUntil - millis()) / 60000 + 1) + " min left)";
    if (armUntil && cfg.armed) t += " (until " + String((int32_t)(armUntil - millis()) / 60000 + 1) + " min from now)";
    if (alertsMuted()) t += ", alerts muted " + String(muteLeftMin()) + " min";
    t += ", " + clockText() + battText() + ", SD " + (live.sdOK ? String(live.freeMB) + " MB free" : String("missing"));
    if (ph >= 0) t += ", phones near " + String(ph);
    if (cfg.campEn) t += ", camp mode";
    reply(t);
  } else if (cmd == "ring") {
    if (!cfg.sirenEn || cfg.sirenPin < 0) reply("the alarm bell is switched off or has no pin.");
    else { int sec = arg.toInt(); if (sec < 1 || sec > 120) sec = cfg.sirenSec; reply("ringing the bell for " + String(sec) + " s."); sirenSound(sec, String("rung over ") + via); }
  } else if (cmd == "quiet") { sirenStop(String("silenced over ") + via); reply("bell silenced."); }
  else if (cmd == "photo") {
    if (strcmp(via, "Telegram") == 0) { reply("snapping one for you."); telegramPhotoNow("221B: the scene at " + clockText()); }
    else { reply("taking photos to the SD card."); cameraRequestPhotos("lora"); }
  }
  else if (cmd == "last") {
    String p = cameraLastPhoto();
    if (!p.length()) reply("no event photo since the last restart.");
    else if (tg) { reply("the latest clue: " + p.substring(p.lastIndexOf('/') + 1)); telegramSendPhotoFile(p, "221B: the latest clue"); }
    else reply("latest photo: " + p.substring(p.lastIndexOf('/') + 1));
  }
  else if (cmd == "log") {
    int n = constrain(arg.toInt() > 0 ? arg.toInt() : (tg ? 5 : 2), 1, tg ? 20 : 3);
    reply("case log:\n" + eventsText(n));
  }
  else if (cmd == "mute") {
    int m = durMin(arg, 60);
    muteFor(m);
    reply("alerts muted for " + durText(m) + ". Still recording everything.");
  }
  else if (cmd == "unmute") { muteFor(0); reply("alerts are back on."); }
  else if (cmd == "audio" || cmd == "video") {
    int sec = arg.toInt();
    if (sec < 1) sec = 5;
    sec = min(sec, cmd == "audio" ? 30 : 20);
    String tag = tg ? "telegram-" + cmd : "lora-" + cmd;
    if (cmd == "audio") audioRequest(tag, sec); else cameraRequestVideo(tag, sec);
    reply("recording " + String(sec) + " s of " + (cmd == "audio" ? "sound" : "video") + (tg ? ", it follows in a moment." : " to the SD card."));
  }
  else if (cmd == "sleep") {
    if (!cfg.campEn) reply("camp mode is off, so the house stays awake.");
    else { reply("going to sleep."); delay(1500); campSleepNow(); }
  } else reply("commands: status, arm [2h], disarm [30], snooze [min], mute [1h], unmute, ring [s], quiet, photo, audio [s], video [s], last, log [n], sleep");
}

static bool isCmd(const String &c) { for (auto k : CMDS) if (c == k) return true; return false; }

// Accepted (whole message, after the radio's "Name: " prefix), case does not matter except the PIN:
//   status | snooze 30            (no PIN set)
//   PIN status | 221B PIN status  (PIN set; "221B" in front is optional)
static void handleLine(String line) {
  line.trim();
  if (!line.length()) return;
  String who, text = line, low = line;
  low.toLowerCase();
  if (low.startsWith("221b:") || low.indexOf(": 221b:") >= 0) return;  // the camera's own replies and alerts
  int colon = line.indexOf(": ");
  if (colon > 0 && colon <= 40) { who = line.substring(0, colon); text = line.substring(colon + 2); }
  who.trim(); text.trim();
  if (who.length() > 20) who = who.substring(0, 20);
  // split into at most 5 words
  String w[5]; int n = 0;
  for (int i = 0; i < (int)text.length() && n < 5;) {
    while (i < (int)text.length() && text[i] == ' ') i++;
    int j = i; while (j < (int)text.length() && text[j] != ' ') j++;
    if (j > i) w[n++] = text.substring(i, j);
    i = j;
  }
  if (!n) return;
  int k = 0;
  String first = w[0]; first.toLowerCase();
  if (first == "221b") k = 1;  // optional old prefix ("221B:" replies from the camera don't match)
  String pin, cmd;
  if (k < n) { cmd = w[k]; cmd.toLowerCase(); }
  if (!isCmd(cmd) && k + 1 < n) { pin = w[k]; cmd = w[k + 1]; cmd.toLowerCase(); k++; }
  if (!isCmd(cmd)) return;
  k++;
  String arg = k < n ? w[k] : String("");
  if (n - k > 1) return;                            // "arm the gate later" is chat, not a command
  if (arg.length() && arg.toInt() <= 0) return;     // the only arguments are numbers (2h, 30m allowed)
  uint32_t now = millis();
  if (!cfg.meshCmdEn) { if (now - lastBadLog > 30000UL) { lastBadLog = now; logEvent("mesh", "LoRa", "ignored a command: two-way is switched off"); } return; }
  if (lockedUntil && (int32_t)(lockedUntil - now) > 0) return;
  if (cfg.meshPin[0] && pin != cfg.meshPin) {
    if (++badPins >= 5) { badPins = 0; lockedUntil = now + 600000UL; logEvent("mesh", "LoRa", "5 wrong PINs: commands locked for 10 minutes"); }
    else if (now - lastBadLog > 30000UL) { lastBadLog = now; logEvent("mesh", "LoRa", String(pin.length() ? "wrong PIN" : "no PIN") + " on a command" + (who.length() ? " from " + who : String(""))); }
    if (!pin.length() && now - lastBadLog < 1000UL) reply("a command PIN is set: send PIN " + cmd + ", or clear the PIN in the settings.");
    return;
  }
  badPins = 0;
  runCommand(cmd, arg, who, "LoRa", loraReply);
}

// Telegram: "status", "/status", "snooze 30"... Returns false if the text is not a command.
bool remoteCommand(String text, const String &who, void (*out)(const String &)) {
  text.trim();
  if (text.startsWith("/")) text = text.substring(1);
  int at = text.indexOf('@');  // "/status@MyBot" in groups
  int sp = text.indexOf(' ');
  if (at > 0 && (sp < 0 || at < sp)) text = text.substring(0, at) + (sp > 0 ? text.substring(sp) : String(""));
  text.trim();
  sp = text.indexOf(' ');
  String cmd = sp < 0 ? text : text.substring(0, sp), arg = sp < 0 ? String("") : text.substring(sp + 1);
  cmd.toLowerCase();
  arg.trim();
  if (cmd == "start") cmd = "help";
  if (!isCmd(cmd) || arg.indexOf(' ') >= 0 || (arg.length() && arg.toInt() <= 0)) return false;
  runCommand(cmd, arg, who, "Telegram", out);
  return true;
}

static void meshCmdTask(void *) {
  String buf;
  for (;;) {
    delay(100);
    if (snoozeUntil && (int32_t)(millis() - snoozeUntil) >= 0) {
      snoozeUntil = 0;
      if (!cfg.armed) {
        setArmed(true, snoozeTg ? "Telegram" : "LoRa", "the snooze is over");
        if (snoozeTg) telegramAlert("221B: snooze over, armed again.", false, true); else reply("snooze over, armed again.");
      }
    }
    if (armUntil && (int32_t)(millis() - armUntil) >= 0) {
      armUntil = 0;
      if (cfg.armed) {
        setArmed(false, armTg ? "Telegram" : "LoRa", "the timed watch is over");
        if (armTg) telegramAlert("221B: timed watch over, disarmed.", false, true); else reply("timed watch over, disarmed.");
      }
    }
    if (!meshActive || cfg.meshRx < 0) { buf = ""; continue; }
    String got;
    xSemaphoreTake(meshLock, portMAX_DELAY);
    while (meshActive && meshSerial.available()) {
      char c = meshSerial.read();
      if (c == '\n' || c == '\r') { if (buf.length()) { got = buf; buf = ""; break; } }
      else if (buf.length() < 240) buf += c;
    }
    xSemaphoreGive(meshLock);
    if (got.length()) handleLine(got);
  }
}

void meshCmdStart() { xTaskCreatePinnedToCore(meshCmdTask, "pigeon", 4096, nullptr, 1, nullptr, 1); }

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
    if (!alertsMuted()) meshSend(String("221B ALARM: movement at ") + clockText() + battText(), true);
  }
}

static void campTask(void *) {
  // only claim the wake pin in camp mode: otherwise D1 may be the alarm bell or a tripwire
  if (cfg.campEn && cfg.campPin >= 0 && cfg.campPin != cfg.sirenPin && cfg.campPin != cfg.irPin)
    pinMode(cfg.campPin, cfg.campLevel ? INPUT_PULLDOWN : INPUT_PULLUP);
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
