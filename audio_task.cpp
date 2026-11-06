// Microphone: continuous level metering, sound trigger, WAV clips with 1 s pre-roll
#include "app.h"
#include "ESP_I2S.h"

#define MIC_CLK_PIN  42
#define MIC_DATA_PIN 41
#define SAMPLE_RATE  16000
#define BLOCK        320   // 20 ms
#define PREROLL_SEC  1

static I2SClass i2s;
static portMUX_TYPE reqMux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t forceUntil = 0;
static char forceTag[24] = "";

struct WavJob { int16_t *buf; size_t samples; String path; };

bool audioInit() {
  i2s.setPinsPdmRx(MIC_CLK_PIN, MIC_DATA_PIN);
  return i2s.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
}

void audioRequest(const String &tag, int seconds) {
  portENTER_CRITICAL(&reqMux);
  if ((int32_t)(forceUntil - millis()) <= 0) strlcpy(forceTag, tag.c_str(), sizeof(forceTag));
  forceUntil = millis() + seconds * 1000UL;
  portEXIT_CRITICAL(&reqMux);
}

static void wavWriter(void *arg) {
  WavJob *job = (WavJob *)arg;
  uint32_t bytes = job->samples * 2;
  uint8_t h[44];
  auto p32 = [&](int o, uint32_t v) { h[o] = v; h[o + 1] = v >> 8; h[o + 2] = v >> 16; h[o + 3] = v >> 24; };
  memcpy(h, "RIFF", 4); p32(4, 36 + bytes); memcpy(h + 8, "WAVEfmt ", 8);
  p32(16, 16); p32(20, 1 | (1 << 16)); p32(24, SAMPLE_RATE); p32(28, SAMPLE_RATE * 2); p32(32, 2 | (16 << 16));
  memcpy(h + 36, "data", 4); p32(40, bytes);
  lockSD();
  File f = openSD(job->path, FILE_WRITE);
  bool ok = f;
  if (f) { f.write(h, 44); f.write((uint8_t *)job->buf, bytes); f.close(); }
  unlockSD();
  logEvent(ok ? "saved" : "error", "microphone",
           String(ok ? "audio " : "could not save ") + job->path.substring(job->path.lastIndexOf('/') + 1) + " (" + String(job->samples / (float)SAMPLE_RATE, 1) + " s)");
  free(job->buf);
  delete job;
  vTaskDelete(nullptr);
}

static void audioTask(void *) {
  static int16_t block[BLOCK];
  const size_t pre = SAMPLE_RATE * PREROLL_SEC;
  int16_t *ring = (int16_t *)ps_malloc(pre * 2);
  size_t head = 0, fill = 0;
  int16_t *clip = nullptr;
  size_t len = 0, cap = 0;
  uint32_t lastLoud = 0, lastEvent = 0;
  String tag;
  float dc = 0;
  bool dcInit = false;

  for (;;) {
    size_t n = i2s.readBytes((char *)block, sizeof(block)) / 2;
    if (!n) continue;
    if (!dcInit) {
      long s = 0;
      for (size_t i = 0; i < n; i++) s += block[i];
      dc = (float)s / n;
      dcInit = true;
    }
    double sq = 0;
    for (size_t i = 0; i < n; i++) {
      dc += (block[i] - dc) * 0.0005f;  // remove the mic's DC offset
      float v = block[i] - dc;
      sq += v * v;
      block[i] = (int16_t)constrain(v * cfg.audioGain, -32768.0f, 32767.0f);
    }
    float db = 20.0f * log10f(sqrtf(sq / n) / 32768.0f + 1e-6f);
    live.micDb = db;
    uint32_t now = millis();
    bool loud = cfg.soundEn && db > cfg.soundDb;
    if (loud && cfg.armed && now - lastEvent > 500) {
      lastEvent = now;
      raiseEvent(SRC_SOUND, "microphone", "noise at " + String(db, 0) + " dB (threshold " + String(cfg.soundDb) + ")");
    }
    bool forced = (int32_t)(forceUntil - now) > 0;

    if (!clip && forced) {
      cap = pre + (size_t)max(cfg.audioMaxSec, cfg.audioTrigSec) * SAMPLE_RATE;
      clip = (int16_t *)ps_malloc(cap * 2);
      if (clip) {
        len = 0;
        for (size_t i = 0; i < fill; i++) clip[len++] = ring[(head + pre - fill + i) % pre];
        fill = 0;
        lastLoud = now;
        portENTER_CRITICAL(&reqMux);
        char t[24];
        strlcpy(t, forceTag, sizeof(t));
        portEXIT_CRITICAL(&reqMux);
        tag = t;
        live.audioRec = true;
      }
    }

    if (clip) {
      size_t cnt = min(n, cap - len);
      memcpy(clip + len, block, cnt * 2);
      len += cnt;
      if (loud && cfg.soundAct.audio) lastLoud = now;
      bool quiet = now - lastLoud > cfg.audioHoldSec * 1000UL && !forced;
      if (quiet || len >= cap) {
        WavJob *job = new WavJob{clip, len, evidencePath(stamp() + "_" + tag + ".wav")};
        xTaskCreate(wavWriter, "wav", 6144, job, 2, nullptr);
        clip = nullptr;
        live.audioRec = false;
      }
    } else if (ring) {
      for (size_t i = 0; i < n; i++) { ring[head] = block[i]; head = (head + 1) % pre; }
      fill = min(fill + n, pre);
    }
  }
}

void audioStart() { xTaskCreatePinnedToCore(audioTask, "audio", 8192, nullptr, 5, nullptr, 1); }
