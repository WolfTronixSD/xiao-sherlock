// Telegram alerts: a message (and the current photo) to your phone, wherever it is.
// Create a bot with @BotFather, paste its token in the web settings, message the bot once,
// then press "Find my chat ID".
#include "app.h"
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include <ArduinoJson.h>

struct Alert { String text; std::vector<uint8_t> photo; String file; };  // file: a path on the SD card
static QueueHandle_t q = xQueueCreate(4, sizeof(Alert *));
static uint32_t lastSent = 0;

// Note: the certificate is not checked (setInsecure) to keep this small. The token is still
// sent encrypted, but someone able to intercept your network could impersonate Telegram.
static bool connect(NetworkClientSecure &c) {
  c.setInsecure();
  c.setTimeout(15000);  // milliseconds
  return c.connect("api.telegram.org", 443);
}

static String readResponse(NetworkClientSecure &c) {
  uint32_t start = millis();
  String body;
  bool inBody = false;
  while ((c.connected() || c.available()) && millis() - start < 15000) {
    if (!c.available()) { delay(10); continue; }
    String line = c.readStringUntil('\n');
    if (inBody) body += line;
    else if (line == "\r") inBody = true;
  }
  return body;
}

static bool sendMessage(const String &text, String &err) {
  NetworkClientSecure c;
  if (!connect(c)) { err = "cannot reach api.telegram.org"; return false; }
  JsonDocument d;
  d["chat_id"] = cfg.tgChat;
  d["text"] = text;
  String body;
  serializeJson(d, body);
  c.printf("POST /bot%s/sendMessage HTTP/1.1\r\nHost: api.telegram.org\r\nContent-Type: application/json\r\n"
           "Content-Length: %u\r\nConnection: close\r\n\r\n", cfg.tgToken, body.length());
  c.print(body);
  String r = readResponse(c);
  if (r.indexOf("\"ok\":true") < 0) { err = r.substring(0, 160); return false; }
  return true;
}

static bool sendPhoto(const String &caption, const std::vector<uint8_t> &jpg, String &err) {
  NetworkClientSecure c;
  if (!connect(c)) { err = "cannot reach api.telegram.org"; return false; }
  const char *B = "----221BakerStreet";
  String head = String("--") + B + "\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + cfg.tgChat + "\r\n" +
                "--" + B + "\r\nContent-Disposition: form-data; name=\"caption\"\r\n\r\n" + caption.substring(0, 1000) + "\r\n" +
                "--" + B + "\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"clue.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
  String tail = String("\r\n--") + B + "--\r\n";
  c.printf("POST /bot%s/sendPhoto HTTP/1.1\r\nHost: api.telegram.org\r\nContent-Type: multipart/form-data; boundary=%s\r\n"
           "Content-Length: %u\r\nConnection: close\r\n\r\n", cfg.tgToken, B, head.length() + jpg.size() + tail.length());
  c.print(head);
  for (size_t i = 0; i < jpg.size(); i += 2048) c.write(jpg.data() + i, min((size_t)2048, jpg.size() - i));
  c.print(tail);
  String r = readResponse(c);
  if (r.indexOf("\"ok\":true") < 0) { err = r.substring(0, 160); return false; }
  return true;
}

// A file from the card (WAV or AVI) as a Telegram document, read and sent in 4 KB pieces
static bool sendFile(const String &caption, const String &path, String &err) {
  lockSD();
  File f = openSD(path, FILE_READ);
  size_t size = f ? f.size() : 0;
  unlockSD();
  if (!f) { err = "file not found: " + path; return false; }
  if (size > 45UL * 1024 * 1024) { lockSD(); f.close(); unlockSD(); err = "file too big for Telegram"; return false; }
  NetworkClientSecure c;
  if (!connect(c)) { lockSD(); f.close(); unlockSD(); err = "cannot reach api.telegram.org"; return false; }
  String name = path.substring(path.lastIndexOf('/') + 1);
  const char *B = "----221BakerStreet";
  String head = String("--") + B + "\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + cfg.tgChat + "\r\n" +
                "--" + B + "\r\nContent-Disposition: form-data; name=\"caption\"\r\n\r\n" + caption.substring(0, 1000) + "\r\n" +
                "--" + B + "\r\nContent-Disposition: form-data; name=\"document\"; filename=\"" + name + "\"\r\nContent-Type: application/octet-stream\r\n\r\n";
  String tail = String("\r\n--") + B + "--\r\n";
  c.printf("POST /bot%s/sendDocument HTTP/1.1\r\nHost: api.telegram.org\r\nContent-Type: multipart/form-data; boundary=%s\r\n"
           "Content-Length: %u\r\nConnection: close\r\n\r\n", cfg.tgToken, B, head.length() + size + tail.length());
  c.print(head);
  static uint8_t buf[4096];
  size_t left = size;
  bool ok = true;
  while (left) {
    lockSD();
    size_t n = f.read(buf, min(left, sizeof(buf)));
    unlockSD();
    if (!n || c.write(buf, n) != n) { ok = false; break; }
    left -= n;
  }
  lockSD();
  f.close();
  unlockSD();
  if (!ok) { err = "upload cut off"; return false; }
  c.print(tail);
  String r = readResponse(c);
  if (r.indexOf("\"ok\":true") < 0) { err = r.substring(0, 160); return false; }
  return true;
}

// ---------- commands from your chat ("status", "/arm", "snooze 30"...) ----------
// Long polling: one getUpdates request waits up to 25 s for a new message, so commands arrive at once.
// Only one TLS connection is open at a time: a waiting alert cuts the poll short and goes first.
static int64_t lastUpdate = 0;   // highest update_id handled
static uint32_t lowHeapLog = 0, pollFailLog = 0;

static void tgReply(const String &t) {
  String err;
  if (!sendMessage("221B: " + t, err)) logEvent("error", "telegram", "reply failed: " + err);
}

static void handleUpdates(const String &body) {
  JsonDocument filter;
  filter["ok"] = true;
  filter["result"][0]["update_id"] = true;
  filter["result"][0]["message"]["chat"]["id"] = true;
  filter["result"][0]["message"]["from"]["first_name"] = true;
  filter["result"][0]["message"]["date"] = true;
  filter["result"][0]["message"]["text"] = true;
  JsonDocument d;
  if (deserializeJson(d, body, DeserializationOption::Filter(filter)) || !d["ok"]) return;
  for (JsonVariant u : d["result"].as<JsonArray>()) {
    int64_t id = u["update_id"].as<int64_t>();
    if (id <= lastUpdate) continue;
    lastUpdate = id;
    JsonVariant m = u["message"];
    if (m.isNull()) continue;
    String chat = m["chat"]["id"].as<String>(), text = m["text"].as<String>(), who = m["from"]["first_name"].as<String>();
    if (chat != cfg.tgChat) { logEvent("telegram", "Telegram", "ignored a message from another chat (" + chat + ")"); continue; }
    // after a restart, Telegram hands over what was sent while the camera was off: skip anything older than 2 minutes
    time_t now = time(nullptr), sent = (time_t)m["date"].as<int64_t>();
    if (timeValid() && sent > 0 && now - sent > 120) { logEvent("telegram", "Telegram", "skipped an old command: " + text.substring(0, 40)); continue; }
    if (who.length() > 20) who = who.substring(0, 20);
    if (!remoteCommand(text, who, tgReply)) tgReply("I only know: arm, disarm, status, ring [s], quiet, snooze [min], photo, audio [s], video [s], sleep, help");
  }
}

// One long poll. Returns early (and drops the connection) as soon as an alert is waiting in the queue.
static void pollOnce() {
  NetworkClientSecure c;
  if (!connect(c)) { if (millis() - pollFailLog > 600000UL) { pollFailLog = millis(); logEvent("error", "telegram", "cannot reach api.telegram.org for commands"); } delay(5000); return; }
  c.printf("GET /bot%s/getUpdates?offset=%lld&limit=5&timeout=25&allowed_updates=%%5B%%22message%%22%%5D HTTP/1.1\r\n"
           "Host: api.telegram.org\r\nConnection: close\r\n\r\n", cfg.tgToken, (long long)(lastUpdate + 1));
  uint32_t start = millis();
  String body, line;
  bool inBody = false;
  while ((c.connected() || c.available()) && millis() - start < 35000) {
    if (uxQueueMessagesWaiting(q)) { c.stop(); return; }  // an alert goes first; the same messages come again next poll
    if (!c.available()) { delay(20); continue; }
    char ch = c.read();
    if (inBody) { if (body.length() < 8192) body += ch; continue; }
    if (ch == '\n') { if (line == "\r" || line.isEmpty()) inBody = true; line = ""; }
    else line += ch;
  }
  c.stop();
  if (body.length()) handleUpdates(body);
}

static bool commandsOn() { return cfg.tgCmdEn && telegramReady() && WiFi.status() == WL_CONNECTED; }

static void telegramTask(void *) {
  Alert *a;
  for (;;) {
    if (xQueueReceive(q, &a, commandsOn() ? 0 : pdMS_TO_TICKS(5000)) == pdTRUE) {
      String err;
      bool ok = a->file.length() ? sendFile(a->text, a->file, err) : a->photo.size() ? sendPhoto(a->text, a->photo, err) : sendMessage(a->text, err);
      if (!ok) logEvent("error", "telegram", err);
      delete a;
      continue;
    }
    if (!commandsOn()) continue;
    // a TLS connection needs about 40 KB: leave the camera and recording enough room
    if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < 50000) {
      if (millis() - lowHeapLog > 600000UL) { lowHeapLog = millis(); logEvent("warn", "telegram", "memory is low, not listening for commands right now"); }
      delay(5000);
      continue;
    }
    pollOnce();
  }
}

// Priority 0 (same as the idle task): a TLS send can busy-wait on a full socket, and it must not starve the watchdog
void telegramStart() { xTaskCreatePinnedToCore(telegramTask, "telegram", 8192, nullptr, 0, nullptr, 0); }

bool telegramReady() { return cfg.tgEn && cfg.tgToken[0] && cfg.tgChat[0]; }

// force = ignore the minimum gap (used by the test button)
void telegramAlert(const String &text, bool withPhoto, bool force) {
  if (!telegramReady() || WiFi.status() != WL_CONNECTED) return;
  uint32_t now = millis();
  if (!force && lastSent && now - lastSent < cfg.tgGapSec * 1000UL) return;
  lastSent = now;
  Alert *a = new Alert{text, {}, ""};
  if (withPhoto && cfg.tgPhoto) cameraSnapshot(a->photo);
  if (xQueueSend(q, &a, 0) != pdTRUE) delete a;
}

// The "photo" command from your chat: always with a photo, ignoring the gap
void telegramPhotoNow(const String &caption) {
  if (!telegramReady()) return;
  Alert *a = new Alert{caption, {}, ""};
  cameraSnapshot(a->photo);
  // keep it as evidence too
  if (a->photo.size() && live.sdOK) {
    String path = evidencePath(stamp() + "_telegram-photo.jpg");
    lockSD();
    File f = openSD(path, FILE_WRITE);
    if (f) { f.write(a->photo.data(), a->photo.size()); f.close(); }
    unlockSD();
    logEvent(f ? "saved" : "error", "telegram", String(f ? "photo " : "could not save ") + path.substring(path.lastIndexOf('/') + 1));
  }
  if (xQueueSend(q, &a, 0) != pdTRUE) delete a;
}

void telegramSendFile(const String &path, const String &caption) {
  if (!telegramReady()) return;
  Alert *a = new Alert{caption, {}, path};
  if (xQueueSend(q, &a, pdMS_TO_TICKS(1000)) != pdTRUE) delete a;
}

// Ask Telegram who has messaged the bot recently, to fill in the chat ID
String telegramFindChat() {
  if (!cfg.tgToken[0]) return "First enter the bot token and save.";
  NetworkClientSecure c;
  if (!connect(c)) return "Cannot reach api.telegram.org";
  c.printf("GET /bot%s/getUpdates?limit=10 HTTP/1.1\r\nHost: api.telegram.org\r\nConnection: close\r\n\r\n", cfg.tgToken);
  String r = readResponse(c);
  JsonDocument d;
  if (deserializeJson(d, r) || !d["ok"]) return "Telegram said: " + r.substring(0, 120);
  JsonArray res = d["result"];
  for (int i = res.size() - 1; i >= 0; i--) {
    JsonVariant chat = res[i]["message"]["chat"];
    if (!chat.isNull()) {
      String id = chat["id"].as<String>();
      strlcpy(cfg.tgChat, id.c_str(), sizeof(cfg.tgChat));
      saveConfig();
      return "OK:" + id + ":" + chat["first_name"].as<String>();
    }
  }
  return "No messages yet. Open your bot in Telegram, send it any message, then try again.";
}
