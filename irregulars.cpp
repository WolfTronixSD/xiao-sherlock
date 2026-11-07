// The Baker Street Irregulars: Holmes's street urchins who notice every stranger.
// Counts phones, watches and trackers nearby from their Bluetooth (BLE) adverts and Wi-Fi probe requests.
// Out in the wild the radio background is empty, so new devices mean someone is near.
// Phones change their addresses every few minutes, so this works on COUNTS over time, not identities.
// Only a scrambled 32-bit ID, signal strength and times are kept, in RAM; nothing leaves the board.
#include "app.h"
#include <WiFi.h>
#include <algorithm>
#include "esp_wifi.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define IRR_MAX 48
#define IRR_BLE 1
#define IRR_WIFI 2

struct Seen { uint32_t id; int8_t rssi; uint8_t kind; uint32_t first, last; };
static Seen seen[IRR_MAX];
static portMUX_TYPE seenMux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool dirty = true;
static bool bleOn = false, wifiOn = false;
static int nowCount = 0;

static uint32_t fnv(const uint8_t *p, size_t n, uint32_t h = 2166136261u) {
  for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
  return h;
}

static void note(uint32_t id, int rssi, uint8_t kind) {
  uint32_t t = millis();
  portENTER_CRITICAL(&seenMux);
  int slot = -1, oldest = 0;
  for (int i = 0; i < IRR_MAX; i++) {
    if (seen[i].id == id && seen[i].kind == kind) { slot = i; break; }
    if (seen[i].last < seen[oldest].last) oldest = i;
  }
  if (slot < 0) { slot = oldest; seen[slot] = {id, (int8_t)rssi, kind, t, t}; }
  else { seen[slot].rssi = (int8_t)((seen[slot].rssi * 3 + rssi) / 4); seen[slot].last = t; }  // smooth the signal a little
  portEXIT_CRITICAL(&seenMux);
}

// ---------- Wi-Fi probe requests (listen-only on the current channel: phones probe on every channel) ----------
static void sniff(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  const wifi_promiscuous_pkt_t *p = (const wifi_promiscuous_pkt_t *)buf;
  if (p->rx_ctrl.sig_len < 24 || p->payload[0] != 0x40) return;  // frame control: management, probe request
  note(fnv(p->payload + 10, 6), p->rx_ctrl.rssi, IRR_WIFI);       // address 2 = the sender
}

static void wifiListen(bool on) {
  wifi_mode_t m;
  if (esp_wifi_get_mode(&m) != ESP_OK || m == WIFI_MODE_NULL) on = false;  // Wi-Fi is off (quiet camp wake)
  bool cur = false;
  esp_wifi_get_promiscuous(&cur);
  if (on && !cur) {
    wifi_promiscuous_filter_t f = {};
    f.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
    esp_wifi_set_promiscuous_filter(&f);
    esp_wifi_set_promiscuous_rx_cb(&sniff);
    esp_wifi_set_promiscuous(true);
  } else if (!on && cur) {
    esp_wifi_set_promiscuous(false);
  }
  wifiOn = on;
}

// ---------- Bluetooth LE adverts ----------
class Watcher : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice d) override {
    String a = d.getAddress().toString().c_str();
    note(fnv((const uint8_t *)a.c_str(), a.length()), d.getRSSI(), IRR_BLE);
  }
};
static Watcher watcher;
static BLEScan *scan = nullptr;

static void bleListen(bool on) {
  if (on && !bleOn) {
    BLEDevice::init("");
    scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(&watcher, true);  // every advert, so the signal stays fresh
    scan->setActiveScan(false);                           // listen only, never ask devices anything
    scan->setInterval(160);
    scan->setWindow(80);  // half duty: leaves air time for Wi-Fi
    bleOn = true;
    logEvent("irregulars", "Irregulars", "listening to Bluetooth, free memory now " + String(ESP.getFreeHeap() / 1024) + " KB");
  } else if (!on && bleOn) {
    scan->stop();
    BLEDevice::deinit(false);
    scan = nullptr;
    bleOn = false;
  }
}

// ---------- counting ----------
// A device counts as "here" while heard recently: BLE adverts come every second or so, Wi-Fi probes far less often
static int countHere(uint32_t t, int *strongest) {
  int n = 0, best = -127;
  portENTER_CRITICAL(&seenMux);
  for (auto &s : seen) {
    if (!s.id || s.rssi < cfg.irrRssi) continue;
    uint32_t window = s.kind == IRR_BLE ? 12000UL : 45000UL;
    if (t - s.last > window) continue;
    n++;
    if (s.rssi > best) best = s.rssi;
  }
  portEXIT_CRITICAL(&seenMux);
  if (strongest) *strongest = best;
  return n;
}

int irregularsNow() { return cfg.irrEn ? nowCount : -1; }

String irregularsJson() {
  uint32_t t = millis();
  Seen copy[IRR_MAX];
  portENTER_CRITICAL(&seenMux);
  memcpy(copy, seen, sizeof(copy));
  portEXIT_CRITICAL(&seenMux);
  std::sort(copy, copy + IRR_MAX, [](const Seen &a, const Seen &b) { return a.last > b.last; });
  String j = "{\"en\":" + String(cfg.irrEn ? "true" : "false") + ",\"ble\":" + (bleOn ? "true" : "false") +
             ",\"wifi\":" + (wifiOn ? "true" : "false") + ",\"now\":" + nowCount + ",\"base\":" + cfg.irrBaseline +
             ",\"extra\":" + cfg.irrExtra + ",\"rssi\":" + cfg.irrRssi + ",\"heap\":" + ESP.getFreeHeap() + ",\"list\":[";
  int n = 0;
  for (auto &s : copy) {
    if (!s.id || t - s.last > 120000UL || n >= 12) continue;
    char id[9];
    snprintf(id, sizeof(id), "%08lx", (unsigned long)s.id);
    j += String(n++ ? "," : "") + "{\"k\":\"" + (s.kind == IRR_BLE ? "BLE" : "Wi-Fi") + "\",\"id\":\"" + String(id).substring(0, 4) +
         "\",\"r\":" + s.rssi + ",\"ago\":" + (t - s.last) / 1000 + ",\"for\":" + (s.last - s.first) / 1000 + "}";
  }
  return j + "]}";
}

void irregularsApplyConfig() { dirty = true; }

static void irregularsTask(void *) {
  uint32_t overSince = 0, underSince = 0, lastAssert = 0, scanStarted = 0;
  bool raised = false;
  for (;;) {
    delay(1000);
    uint32_t t = millis();
    if (dirty || t - lastAssert > 30000UL) {  // Wi-Fi restarts can switch listening off; check now and then
      dirty = false;
      lastAssert = t;
      bleListen(cfg.irrEn && cfg.irrBle);
      wifiListen(cfg.irrEn && cfg.irrWifi);
    }
    if (!bleOn) scanStarted = 0;
    else if (!scanStarted || t - scanStarted > 31000UL) {  // a 30 s background scan, started again when it ends
      scan->stop();
      scan->clearResults();
      scan->start(30, nullptr, false);
      scanStarted = t;
    }
    if (!cfg.irrEn) { nowCount = 0; overSince = 0; raised = false; continue; }
    int strongest;
    nowCount = countHere(t, &strongest);
    if (nowCount >= cfg.irrBaseline + cfg.irrExtra) {
      underSince = 0;
      if (!overSince) overSince = t;
      if (!raised && t - overSince >= cfg.irrHoldSec * 1000UL) {
        raised = true;
        raiseEvent(SRC_IRREG, "Irregulars", String(nowCount) + " phones/watches nearby (usually " + cfg.irrBaseline +
                                                "), strongest " + strongest + " dBm");
      }
    } else {
      overSince = 0;
      if (!underSince) underSince = t;
      if (raised && t - underSince > 60000UL) raised = false;  // quiet for a minute: ready to report the next visitor
    }
  }
}

void irregularsStart() { xTaskCreatePinnedToCore(irregularsTask, "irregulars", 6144, nullptr, 1, nullptr, 0); }
