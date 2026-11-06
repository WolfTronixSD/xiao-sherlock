// Telegram alerts: a message (and the current photo) to your phone, wherever it is.
// Create a bot with @BotFather, paste its token in the web settings, message the bot once,
// then press "Find my chat ID".
#include "app.h"
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include <ArduinoJson.h>

struct Alert { String text; std::vector<uint8_t> photo; };
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

static void telegramTask(void *) {
  Alert *a;
  for (;;) {
    if (xQueueReceive(q, &a, portMAX_DELAY) != pdTRUE) continue;
    String err;
    bool ok = a->photo.size() ? sendPhoto(a->text, a->photo, err) : sendMessage(a->text, err);
    if (!ok) logEvent("error", "telegram", err);
    delete a;
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
  Alert *a = new Alert{text, {}};
  if (withPhoto && cfg.tgPhoto) cameraSnapshot(a->photo);
  if (xQueueSend(q, &a, 0) != pdTRUE) delete a;
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
