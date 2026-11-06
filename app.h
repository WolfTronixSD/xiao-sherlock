// 221B Baker Street security recorder - shared declarations
#pragma once
#include <Arduino.h>
#include <vector>
#include "FS.h"
#include "SD.h"

#define FW_VERSION "2.5"
#define SEC_DIR    "/sec"
#define MAX_INPUTS 4
#define MAX_ZONES  6

// ---------- event sources ----------
enum Source { SRC_SOUND, SRC_MOTION, SRC_INPUT1, SRC_INPUT2, SRC_INPUT3, SRC_INPUT4, SRC_RADAR, SRC_SONAR, SRC_TEST, SRC_COUNT };

struct Actions { bool photo, video, audio, notify; };
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
  // Telegram
  bool tgEn = false;
  char tgToken[64] = "";
  char tgChat[24] = "";
  bool tgPhoto = true;
  int  tgGapSec = 30;
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
  String pinWarning;
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

// ---------- web.cpp ----------
void webStart();
void webLoop();
