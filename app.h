// 221B Baker Street security recorder - shared declarations
#pragma once
#include <Arduino.h>
#include <vector>
#include "FS.h"
#include "SD.h"

#define FW_VERSION "3.10.1"
#define SEC_DIR    "/sec"
#define MAX_INPUTS 4
#define MAX_ZONES  6

// ---------- event sources ----------
enum Source { SRC_SOUND, SRC_MOTION, SRC_INPUT1, SRC_INPUT2, SRC_INPUT3, SRC_INPUT4, SRC_RADAR, SRC_SONAR, SRC_IRREG, SRC_TEST, SRC_COUNT };

struct Actions { bool photo, video, audio, notify, siren; };
struct Input   { bool en; char name[20]; int pin; int pull; int edge; Actions act; };
struct Zone    { bool en; char name[20]; float x, y, w, h; int minPct; };

// ---------- configuration (edited from the web GUI, stored in flash) ----------
struct Config {
  // general
  bool armed = true;
  char wifiSsid[33] = "";
  char wifiPass[65] = "";
  char hostname[24] = "baker-street";
  char webPass[33] = "";
  char tz[48] = "PST8PDT,M3.2.0,M11.1.0";
  int  cooldownSec = 10;
  int  debounceMs = 50;
  bool witnessEn = false;   // two witnesses: an alarm needs two different sensors within witnessSec
  int  witnessSec = 8;     // a tripwire pin must hold its new level this long
  bool autoClean = true;
  int  cleanPct = 90;       // delete the oldest day when the card is fuller than this
  // schedule and phone presence
  bool schedEn = false;
  int  schedArm = 2300;     // HHMM
  int  schedDisarm = 700;   // HHMM
  bool homeEn = false;
  char homeIp[16] = "";
  int  homeAwayMin = 15;
  // camera
  int  frameSize = 0;  // set to FRAMESIZE_SVGA in buildFields()
  int  jpegQuality = 12;
  bool vflip = true;
  bool hmirror = false;
  int  photoCount = 3;
  int  photoIntervalMs = 500;
  int  videoSec = 10;
  int  videoMaxSec = 60;
  // sound
  bool soundEn = true;
  int  soundDb = -35;
  int  audioHoldSec = 3;
  int  audioMaxSec = 30;
  int  audioGain = 4;
  int  audioTrigSec = 10;
  Actions soundAct = {true, false, true, false};
  // motion
  bool motionEn = false;
  int  motionPixel = 25;
  int  motionLightPct = 60;
  Actions motionAct = {true, true, false, true};
  Zone zones[MAX_ZONES] = {};
  // GPIO inputs
  Input in[MAX_INPUTS] = {};
  // LD2450 radar
  bool radarEn = false;
  int  radarRx = 44;
  int  radarTx = 43;
  int  radarMaxMm = 3000;
  Actions radarAct = {true, true, false, true};
  // HC-SR04 ultrasonic
  bool sonarEn = false;
  int  sonarTrig = 3;
  int  sonarEcho = 4;
  int  sonarCm = 100;
  Actions sonarAct = {true, false, false, true};
  // the Baker Street Irregulars: phones nearby (Wi-Fi probe requests)
  bool irrEn = false;
  int  irrRssi = -80;       // only count devices stronger than this (dBm)
  int  irrBaseline = 0;     // devices normally around (yours)
  int  irrExtra = 1;        // alarm when this many more than the baseline...
  int  irrHoldSec = 30;     // ...stay for this long
  Actions irrAct = {true, false, false, true};
  // Telegram
  bool tgEn = false;
  char tgToken[64] = "";
  char tgChat[24] = "";
  bool tgPhoto = true;
  int  tgGapSec = 30;
  // night watch (IR lamp + black-and-white picture)
  int  nightMode = 1;       // 0 off, 1 by light level, 2 by the clock, 3 always
  int  nightBelow = 35;     // light level (0-255) under which night begins
  int  nightAbove = 80;     // light level over which day returns
  int  nightFrom = 2000;    // HHMM, for mode 2
  int  nightTo = 700;
  bool nightBW = true;
  int  irPin = -1;
  // the alarm bell (siren, buzzer or LED via a transistor/relay)
  int  sirenPin = -1;
  bool sirenActiveLow = false;
  bool sirenEn = true;      // off: the bell never rings (the pin stays idle)
  int  sirenSec = 10;
  int  sirenStyle = 1;      // 0 steady, 1 slow beep, 2 fast blink, 3 piezo wail, 4 piezo beeps
  // MQTT / Home Assistant
  bool mqttEn = false;
  char mqttHost[48] = "";
  int  mqttPort = 1883;
  char mqttUser[33] = "";
  char mqttPass[65] = "";
  char mqttBase[32] = "baker-street";
  bool mqttHa = true;
  // camp mode (battery: deep sleep, wake on PIR or BOOT, hotspot on demand)
  bool campEn = false;
  int  campPin = 2;         // D1, the default PIR tripwire
  int  campLevel = 1;       // 1 wake when HIGH, 0 when LOW
  int  campAwakeSec = 30;   // stay awake this long after the last movement
  int  campHotspotMin = 10; // after BOOT / power on: stay awake this long without a web visitor
  int  campBeatMin = 0;     // heartbeat over LoRa every N minutes (0 = off)
  int  battPin = -1;        // battery voltage through a divider
  int  battMul = 200;       // divider ratio x100 (two equal resistors = 200)
  // Meshtastic LoRa radio on a UART (serial module, TEXTMSG mode)
  bool meshEn = false;
  int  meshTx = -1;         // ESP TX -> radio RX
  int  meshRx = -1;         // ESP RX <- radio TX (optional)
  int  meshBaud = 38400;
  int  meshGapSec = 60;
  bool meshCmdEn = false;    // two-way: accept commands over LoRa ("221B <pin> <command>")
  char meshPin[17] = "";    // PIN every command must carry
};
extern Config cfg;

// ---------- live state (shown in the web GUI and written to the case log) ----------
struct RadarTarget { bool valid; int x, y, speed; };  // mm, mm, cm/s
struct LiveState {
  volatile float micDb = -96;
  volatile bool audioRec = false;
  volatile bool videoRec = false;
  volatile int  motionPct[MAX_ZONES] = {};
  volatile int  motionGlobal = 0;
  volatile int  inputLevel[MAX_INPUTS] = {-1, -1, -1, -1};
  RadarTarget   radar[3] = {};
  volatile bool radarSeen = false;   // got frames from the radar
  volatile int  sonarCm = -1;
  volatile int  phoneHome = -1;      // -1 not watched, 0 away, 1 home
  bool sdOK = false;
  uint32_t freeMB = 0;
  uint32_t totalMB = 0;
  uint32_t boots = 0;                // restarts since the firmware was first installed
  const char *resetWhy = "?";        // why the last restart happened
  volatile int  light = -1;          // average picture brightness 0-255 (-1 not measured yet)
  volatile bool night = false;       // night watch on (IR lamp lit, picture black and white)
  volatile bool siren = false;       // alarm bell sounding
  volatile int  mqtt = -1;           // -1 off, 0 not connected, 1 connected
  String pinWarning;
  volatile bool saveFailed = false;  // the last settings save did not reach flash
  volatile int nvsFree = -1;         // free NVS entries (32 bytes each)
  String wdtWho;                     // what each core was running when the watchdog bit last time
};
extern LiveState live;

// ---------- config.cpp ----------
struct Field {
  String group, key, label;
  char type;          // b bool, i int, s text, w secret text, p pin, e choice
  void *ptr;
  int min, max;       // int range, or text buffer size
  const char *opts;   // for 'e': "0:Off|1:On"
  const char *help;
};
extern std::vector<Field> fields;
void buildFields();
void loadConfig();
void saveConfig();
String configJson(bool forWeb);
bool applyConfigJson(const String &json, String &err);
String schemaJson();
const char *sourceName(Source s);
Actions actionsFor(Source s);
extern const int  GPIO_PINS[];
extern const char *GPIO_NAMES[];
extern const int  NUM_GPIO;
String pinName(int pin);

// ---------- core.cpp ----------
void lockSD();
void unlockSD();
bool tryLockSD(uint32_t ms);
String evidencePath(const String &name);
String findEvidence(const String &name);
bool mountSD();
File openSD(const String &path, const char *mode);
bool timeValid();
String stamp();
String clockText();
String safeTag(const String &s);
void logEvent(const String &kind, const String &who, const String &detail);
void flushEvents();
String eventsJson(int max);
void raiseEvent(Source src, const String &who, const String &detail);
void setArmed(bool on, const String &who, const String &why);
void janitorStart();
String sensorSnapshot();

// ---------- camera_task.cpp ----------
bool cameraInit();
void cameraApplyConfig();
void cameraStart();
void cameraRequestPhotos(const String &tag);
void cameraRequestVideo(const String &tag);
bool cameraSnapshot(std::vector<uint8_t> &out);
String motionGridJson();

// ---------- audio_task.cpp ----------
bool audioInit();
void audioStart();
void audioRequest(const String &tag, int seconds);

// ---------- sensors.cpp ----------
void sensorsApplyConfig();
void sensorsStart();

// ---------- telegram.cpp ----------
void telegramStart();
bool telegramReady();
void telegramAlert(const String &text, bool withPhoto, bool force);
String telegramFindChat();

// ---------- presence.cpp ----------
void presenceStart();

// ---------- outputs.cpp (night watch, alarm bell, MQTT) ----------
void outputsStart();
void outputsApplyConfig();
void sirenSound(int seconds, const String &why);
void sirenStop(const String &why);
void bellPinsChanged();  // sensors.cpp re-set the output pins
void mqttEvent(const String &kind, const String &who, const String &detail);

// ---------- camp.cpp (camp mode and Meshtastic) ----------
void campEarly();
bool campQuiet();
void campStart();
// irregulars.cpp
void irregularsStart();
void irregularsApplyConfig();
String irregularsJson();
int irregularsNow();
void irregularsMarkMine();
void campSleepNow();
int  campSleepIn();
const char *campWakeWhy();
uint32_t campWakeCount();
void meshBegin();
bool meshSend(const String &text, bool force);
void meshCmdStart();
int  batteryMv();
String battText();

// ---------- web.cpp ----------
uint32_t webLastRequest();
bool webBusy();
int deleteDay(const String &day, bool keepLog);
String oldestDay();
void webStart();
void webLoop();
