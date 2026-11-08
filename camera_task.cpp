// Camera: one task owns the camera and handles photos, AVI video, motion zones and the live snapshot
#include "app.h"
#include "esp_camera.h"
#include "img_converters.h"

#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  10
#define SIOD_GPIO_NUM  40
#define SIOC_GPIO_NUM  39
#define Y9_GPIO_NUM    48
#define Y8_GPIO_NUM    11
#define Y7_GPIO_NUM    12
#define Y6_GPIO_NUM    14
#define Y5_GPIO_NUM    16
#define Y4_GPIO_NUM    18
#define Y3_GPIO_NUM    17
#define Y2_GPIO_NUM    15
#define VSYNC_GPIO_NUM 38
#define HREF_GPIO_NUM  47
#define PCLK_GPIO_NUM  13

static portMUX_TYPE reqMux = portMUX_INITIALIZER_UNLOCKED;
static int photosWanted = 0;
static char photoTag[24];
static uint32_t videoUntil = 0, videoStarted = 0;
static char videoTag[24];
static bool videoWanted = false;
static volatile bool configDirty = false;

static SemaphoreHandle_t snapLock = xSemaphoreCreateMutex();
static uint8_t *snapBuf = nullptr;
static size_t snapLen = 0, snapCap = 0;

bool cameraInit() {
  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM;  c.pin_d1 = Y3_GPIO_NUM;
  c.pin_d2 = Y4_GPIO_NUM;  c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM;  c.pin_d5 = Y7_GPIO_NUM;
  c.pin_d6 = Y8_GPIO_NUM;  c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM;
  c.pin_pclk = PCLK_GPIO_NUM;
  c.pin_vsync = VSYNC_GPIO_NUM;
  c.pin_href = HREF_GPIO_NUM;
  c.pin_sccb_sda = SIOD_GPIO_NUM;
  c.pin_sccb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn = PWDN_GPIO_NUM;
  c.pin_reset = RESET_GPIO_NUM;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;
  c.frame_size = FRAMESIZE_UXGA;  // allocate buffers for the largest size
  c.jpeg_quality = 10;
  c.fb_count = 2;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;
  if (esp_camera_init(&c) != ESP_OK) return false;
  sensor_t *s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) { s->set_brightness(s, 1); s->set_saturation(s, -2); }
  configDirty = true;
  return true;
}

void cameraApplyConfig() { configDirty = true; }

static void applyNow() {
  sensor_t *s = esp_camera_sensor_get();
  if (!s) return;
  s->set_framesize(s, (framesize_t)cfg.frameSize);
  s->set_quality(s, cfg.jpegQuality);
  s->set_vflip(s, cfg.vflip);
  s->set_hmirror(s, cfg.hmirror);
  s->set_special_effect(s, live.night && cfg.nightBW ? 2 : 0);  // 2 = grayscale
}

static char lastPhoto[96] = "";  // first photo of the latest burst, for the "last" command
String cameraLastPhoto() { portENTER_CRITICAL(&reqMux); String s = lastPhoto; portEXIT_CRITICAL(&reqMux); return s; }

void cameraRequestPhotos(const String &tag) {
  portENTER_CRITICAL(&reqMux);
  photosWanted = cfg.photoCount;
  strlcpy(photoTag, tag.c_str(), sizeof(photoTag));
  portEXIT_CRITICAL(&reqMux);
}

void cameraRequestVideo(const String &tag, int seconds) {
  portENTER_CRITICAL(&reqMux);
  uint32_t now = millis();
  if (!videoWanted && !live.videoRec) { videoStarted = now; strlcpy(videoTag, tag.c_str(), sizeof(videoTag)); }
  uint32_t until = now + (seconds > 0 ? seconds : cfg.videoSec) * 1000UL;
  uint32_t cap = videoStarted + cfg.videoMaxSec * 1000UL;
  videoUntil = (int32_t)(until - cap) > 0 ? cap : until;
  videoWanted = true;
  portEXIT_CRITICAL(&reqMux);
}

bool cameraSnapshot(std::vector<uint8_t> &out) {
  xSemaphoreTake(snapLock, portMAX_DELAY);
  bool ok = snapLen > 0;
  if (ok) out.assign(snapBuf, snapBuf + snapLen);
  xSemaphoreGive(snapLock);
  return ok;
}

// ---------- AVI (MJPEG) writer ----------
// RIFF 'AVI ' { LIST 'hdrl' { avih, LIST 'strl' { strh, strf } }, LIST 'movi' { '00dc'... }, idx1 }
#define AVI_HDR 224
static void put32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static void put16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }

struct Avi {
  File file;
  String path;
  uint32_t *index = nullptr, cap = 0, frames = 0, movi = 4, maxFrame = 0, start = 0, first = 0, last = 0;
  uint16_t w = 0, h = 0;

  static void header(uint8_t *b, uint32_t frames, uint32_t fpsMilli, uint16_t w, uint16_t h, uint32_t movi, uint32_t idx, uint32_t maxFrame) {
    memset(b, 0, AVI_HDR);
    memcpy(b, "RIFF", 4);        put32(b + 4, AVI_HDR - 12 + movi + 8 + idx);
    memcpy(b + 8, "AVI LIST", 8); put32(b + 16, 192); memcpy(b + 20, "hdrlavih", 8); put32(b + 28, 56);
    put32(b + 32, fpsMilli ? 1000000000UL / fpsMilli : 100000);
    put32(b + 44, 0x10); put32(b + 48, frames); put32(b + 56, 1); put32(b + 60, maxFrame);
    put32(b + 64, w); put32(b + 68, h);
    memcpy(b + 88, "LIST", 4);   put32(b + 92, 116); memcpy(b + 96, "strlstrh", 8); put32(b + 104, 56);
    memcpy(b + 108, "vidsMJPG", 8);
    put32(b + 128, 1000); put32(b + 132, fpsMilli); put32(b + 140, frames); put32(b + 144, maxFrame);
    put32(b + 148, 0xFFFFFFFF); put16(b + 160, w); put16(b + 162, h);
    memcpy(b + 164, "strf", 4);  put32(b + 168, 40);
    put32(b + 172, 40); put32(b + 176, w); put32(b + 180, h); put16(b + 184, 1); put16(b + 186, 24);
    memcpy(b + 188, "MJPG", 4);  put32(b + 192, (uint32_t)w * h * 3);
    memcpy(b + 212, "LIST", 4);  put32(b + 216, movi); memcpy(b + 220, "movi", 4);
  }

  bool begin(const String &p) {
    path = p; frames = 0; movi = 4; maxFrame = 0; w = h = 0; start = millis(); first = last = 0;
    uint8_t b[AVI_HDR];
    header(b, 0, 0, 0, 0, 4, 0, 0);
    lockSD();
    file = openSD(path, FILE_WRITE);
    if (file) file.write(b, AVI_HDR);
    unlockSD();
    return (bool)file;
  }

  void add(const uint8_t *buf, uint32_t len, uint16_t fw, uint16_t fh, uint32_t ms) {
    if (frames >= cap) {
      uint32_t nc = cap ? cap * 2 : 512;
      uint32_t *ni = (uint32_t *)ps_realloc(index, nc * 8);
      if (!ni) return;
      index = ni; cap = nc;
    }
    if (!w) { w = fw; h = fh; }
    if (!frames) first = ms;
    last = ms;
    uint32_t padded = (len + 1) & ~1UL;
    uint8_t c[8];
    memcpy(c, "00dc", 4); put32(c + 4, len);
    lockSD();
    file.write(c, 8);
    file.write(buf, len);
    if (padded > len) file.write((uint8_t)0);
    unlockSD();
    index[frames * 2] = movi; index[frames * 2 + 1] = len;
    movi += 8 + padded;
    if (len > maxFrame) maxFrame = len;
    frames++;
  }

  float finish() {
    // the pace the pictures were taken at (not how long the card took to write them)
    uint32_t ms = frames > 1 ? last - first : 0;
    float fps = ms ? (frames - 1) * 1000.0f / ms : (frames ? 1.0f : 0);
    uint8_t b[AVI_HDR];
    lockSD();
    uint8_t e[16];
    memcpy(e, "idx1", 4); put32(e + 4, frames * 16); file.write(e, 8);
    for (uint32_t i = 0; i < frames; i++) {
      memcpy(e, "00dc", 4); put32(e + 4, 0x10); put32(e + 8, index[i * 2]); put32(e + 12, index[i * 2 + 1]);
      file.write(e, 16);
    }
    header(b, frames, (uint32_t)(fps * 1000), w, h, movi, frames * 16, maxFrame);
    file.seek(0);
    file.write(b, AVI_HDR);
    file.close();
    unlockSD();
    free(index); index = nullptr; cap = 0;
    return fps;
  }
};

// ---------- the evidence writer ----------
// Creating a file on a busy card can take 2 s, so the camera loop never touches the SD card: it copies
// each picture to PSRAM and hands it to this task, which writes photos and video in order.
enum JobType : uint8_t { JOB_PHOTO, JOB_AVI_BEGIN, JOB_FRAME, JOB_AVI_END };
struct Job {
  JobType type;
  uint8_t *buf;
  uint32_t len, ms;
  uint16_t w, h;
  int8_t n, of;     // photo n of "of"
  char path[96];
};
static QueueHandle_t jobs = nullptr;
static volatile int jobsQueued = 0;
static volatile bool writerVideo = false;  // the writer still has a video open

static bool postJob(JobType t, camera_fb_t *fb, const String &path = "", int n = 0, int of = 0) {
  if (!jobs) return false;
  Job *j = (Job *)ps_malloc(sizeof(Job));
  if (!j) return false;
  memset(j, 0, sizeof(Job));
  j->type = t; j->n = n; j->of = of; j->ms = millis();
  strlcpy(j->path, path.c_str(), sizeof(j->path));
  if (fb) {
    // keep at least 1 MB of PSRAM for everything else; drop the picture rather than run dry
    if (ESP.getFreePsram() < 1024 * 1024 + fb->len || !(j->buf = (uint8_t *)ps_malloc(fb->len))) { free(j); return false; }
    memcpy(j->buf, fb->buf, fb->len);
    j->len = fb->len; j->w = fb->width; j->h = fb->height;
  }
  if (xQueueSend(jobs, &j, t == JOB_FRAME ? 0 : pdMS_TO_TICKS(200)) != pdTRUE) { free(j->buf); free(j); return false; }
  jobsQueued++;
  return true;
}

static void writerTask(void *) {
  Avi avi;
  bool open = false;
  for (;;) {
    Job *j;
    if (xQueueReceive(jobs, &j, portMAX_DELAY) != pdTRUE) continue;
    jobsQueued--;
    switch (j->type) {
      case JOB_AVI_BEGIN:
        if (open) avi.finish();
        open = avi.begin(j->path);
        writerVideo = open;
        if (!open) logEvent("error", "camera", "could not create video file");
        break;
      case JOB_FRAME:
        if (open) avi.add(j->buf, j->len, j->w, j->h, j->ms);
        break;
      case JOB_AVI_END:
        if (open) {
          float fps = avi.finish();
          open = false;
          logEvent("saved", "camera", "video " + avi.path.substring(avi.path.lastIndexOf('/') + 1) + " (" + String(avi.frames) + " frames, " + String(fps, 1) + " fps)");
          if (avi.path.indexOf("_telegram-") >= 0) telegramSendFile(avi.path, "221B: the moving picture you asked for");
        }
        writerVideo = false;
        break;
      case JOB_PHOTO: {
        lockSD();
        File f = openSD(j->path, FILE_WRITE);
        bool ok = f;
        if (f) { f.write(j->buf, j->len); f.close(); }
        unlockSD();
        if (!ok) logEvent("error", "camera", "could not save photo");
        if (ok && j->n == 1) { portENTER_CRITICAL(&reqMux); strlcpy(lastPhoto, j->path, sizeof(lastPhoto)); portEXIT_CRITICAL(&reqMux); }
        if (j->n == j->of) {
          String base = String(j->path);
          base = base.substring(base.lastIndexOf('/') + 1, base.lastIndexOf('_'));
          logEvent("saved", "camera", String(j->of) + " photo(s) " + base + "_*.jpg");
        }
        break;
      }
    }
    free(j->buf);
    free(j);
    vTaskDelay(2);  // a slow card busy-waits the SPI bus: let the idle task in, or the watchdog restarts the board
  }
}

// ---------- motion detection ----------
// Each analysed frame is decoded at 1/8 size, turned into grey (its average is the light level
// for the night watch), and compared with a slowly adapting background. Changed pixels are counted per zone.
static uint8_t *rgb = nullptr, *grey = nullptr, *bg = nullptr, *mask = nullptr;
static int mw = 0, mh = 0;
static bool bgValid = false;
static uint32_t motionSince[MAX_ZONES] = {};

static void analyse(camera_fb_t *fb) {
  int w = fb->width / 8, h = fb->height / 8;
  if (w != mw || h != mh) {
    free(rgb); free(grey); free(bg); free(mask);
    rgb = (uint8_t *)ps_malloc(w * h * 2); grey = (uint8_t *)ps_malloc(w * h);
    bg = (uint8_t *)ps_malloc(w * h);      mask = (uint8_t *)ps_malloc(w * h);
    mw = w; mh = h; bgValid = false;
    if (!rgb || !grey || !bg || !mask) { mw = mh = 0; return; }
  }
  if (!jpg2rgb565(fb->buf, fb->len, rgb, JPG_SCALE_8X)) return;
  int n = w * h;
  for (int i = 0; i < n; i++) {
    uint16_t c = (rgb[2 * i] << 8) | rgb[2 * i + 1];
    int r = (c >> 11) << 3, g = ((c >> 5) & 63) << 2, b = (c & 31) << 3;
    grey[i] = (r * 77 + g * 150 + b * 29) >> 8;
  }
  uint32_t sum = 0;
  for (int i = 0; i < n; i++) sum += grey[i];
  if (!ldrLight()) live.light = sum / n;  // for the night watch (unless an LDR on the LIS3DH does it)
  if (!cfg.motionEn) { bgValid = false; return; }
  if (!bgValid) { memcpy(bg, grey, n); memset(mask, 0, n); bgValid = true; return; }

  int changed = 0;
  int thr = cfg.motionPixel;
  for (int i = 0; i < n; i++) {
    mask[i] = abs(grey[i] - bg[i]) > thr;
    changed += mask[i];
  }
  int global = changed * 100 / n;
  live.motionGlobal = global;
  bool lightChange = global > cfg.motionLightPct;

  bool anyZone = false;
  for (auto &z : cfg.zones) anyZone |= z.en;
  for (int zi = 0; zi < MAX_ZONES; zi++) {
    Zone z = cfg.zones[zi];
    if (!anyZone && zi == 0) { z = {true, "Whole picture", 0, 0, 1, 1, 5}; }
    else if (!z.en) { live.motionPct[zi] = 0; continue; }
    int x0 = z.x * w, y0 = z.y * h, x1 = (z.x + z.w) * w, y1 = (z.y + z.h) * h;
    x1 = min(x1, w); y1 = min(y1, h);
    int cnt = 0, tot = max(1, (x1 - x0) * (y1 - y0));
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++) cnt += mask[y * w + x];
    int pct = cnt * 100 / tot;
    live.motionPct[zi] = pct;
    // require the change to be seen on two analyses in a row to ignore single-frame glitches
    if (!lightChange && pct >= z.minPct) {
      if (!motionSince[zi]) motionSince[zi] = millis();
      else raiseEvent(SRC_MOTION, z.name, String(pct) + "% of the zone changed");
    } else {
      motionSince[zi] = 0;
    }
  }
  // adapt the background: fast after a light change, slowly otherwise
  int rate = lightChange ? 2 : 4;
  for (int i = 0; i < n; i++) bg[i] += (grey[i] - bg[i]) >> rate;
}

String motionGridJson() {
  // coarse grid (cells of 4x4 analysis pixels) of changed fraction 0-9, for the web overlay
  String s = "{\"w\":" + String(mw / 4) + ",\"h\":" + String(mh / 4) + ",\"g\":\"";
  if (mask && bgValid) {
    for (int cy = 0; cy < mh / 4; cy++)
      for (int cx = 0; cx < mw / 4; cx++) {
        int c = 0;
        for (int y = 0; y < 4; y++)
          for (int x = 0; x < 4; x++) c += mask[(cy * 4 + y) * mw + cx * 4 + x];
        s += (char)('0' + c * 9 / 16);
      }
  }
  return s + "\"}";
}

// ---------- the camera task ----------
static void cameraTask(void *) {
  bool aviOpen = false;
  uint32_t dropped = 0;
  String photoBase;
  int photoIdx = 0;
  uint32_t nextPhoto = 0, lastSnap = 0, lastMotion = 0, frames = 0;
  int failures = 0;
  logEvent("camera", "camera", "task started");

  uint32_t paceAt = millis(), paceFrames = 0, grabMax = 0, loopMax = 0;
  for (;;) {
    if (configDirty) { configDirty = false; applyNow(); bgValid = false; delay(200); }
    uint32_t t0 = millis();
    camera_fb_t *fb = esp_camera_fb_get();
    uint32_t t1 = millis();
    if (t1 - t0 > grabMax) grabMax = t1 - t0;
    if (t1 - paceAt >= 5000) {  // the pace of the camera, for the Doctor's Report
      live.camFps = paceFrames * 1000.0f / (t1 - paceAt);
      live.camGrabMs = grabMax; live.camLoopMs = loopMax;
      paceAt = t1; paceFrames = 0; grabMax = loopMax = 0;
    }
    if (!fb) {
      if (++failures == 5) logEvent("error", "camera", "no picture from the camera");
      if (failures % 15 == 0) {  // still nothing: start the camera again (it can stay stuck after a soft restart)
        static int restarts = 0;
        esp_camera_deinit();
        delay(300);
        bool ok = cameraInit();
        restarts = ok ? 0 : restarts + 1;
        if (ok) logEvent("camera", "camera", "camera restarted after no pictures");
        else if (restarts == 1 || restarts % 30 == 0)  // don't flood the case log while the camera is missing
          logEvent("error", "camera", "camera restart failed (" + String(restarts) + "x) - is the camera board seated?");
        bgValid = false;
      }
      delay(failures >= 15 ? 2000 : 20);  // a missing camera answers at once: retry slowly
      continue;
    }
    if (failures >= 5 || !frames) logEvent("camera", "camera", "pictures arriving, " + String(fb->width) + "x" + String(fb->height));
    failures = 0;
    frames++;
    paceFrames++;
    uint32_t now = millis();

    if (now - lastSnap >= 100) {  // keep a copy for the web live view
      lastSnap = now;
      xSemaphoreTake(snapLock, portMAX_DELAY);
      if (fb->len > snapCap) { free(snapBuf); snapCap = fb->len + 16384; snapBuf = (uint8_t *)ps_malloc(snapCap); }
      if (snapBuf) { memcpy(snapBuf, fb->buf, fb->len); snapLen = fb->len; }
      xSemaphoreGive(snapLock);
    }

    // motion analysis 4x a second; with motion off still measure the light every 2 s for the night watch
    if (now - lastMotion >= (cfg.motionEn ? 250u : 2000u)) { lastMotion = now; analyse(fb); }

    // video
    portENTER_CRITICAL(&reqMux);
    bool wantVideo = videoWanted && (int32_t)(videoUntil - now) > 0;
    if (videoWanted && !wantVideo) videoWanted = false;
    char vtag[24];
    strlcpy(vtag, videoTag, sizeof(vtag));
    portEXIT_CRITICAL(&reqMux);
    if (wantVideo && !aviOpen) {
      aviOpen = postJob(JOB_AVI_BEGIN, nullptr, evidencePath(stamp() + "_" + vtag + ".avi"));
      if (!aviOpen) { logEvent("error", "camera", "could not start a video (writer busy)"); portENTER_CRITICAL(&reqMux); videoWanted = false; portEXIT_CRITICAL(&reqMux); }
    }
    if (aviOpen && wantVideo && !postJob(JOB_FRAME, fb)) dropped++;
    if (aviOpen && !wantVideo) {
      while (!postJob(JOB_AVI_END, nullptr)) delay(50);  // the end must arrive, or the file stays open
      aviOpen = false;
      if (dropped) logEvent("warning", "camera", "video: " + String(dropped) + " picture(s) dropped, the card could not keep up");
      dropped = 0;
    }
    live.videoRec = aviOpen || writerVideo;

    // photos
    portENTER_CRITICAL(&reqMux);
    int want = photosWanted;
    char ptag[24];
    strlcpy(ptag, photoTag, sizeof(ptag));
    if (want) photosWanted = 0;
    portEXIT_CRITICAL(&reqMux);
    if (want) { photoBase = evidencePath(stamp() + "_" + ptag); photoIdx = want; nextPhoto = now; }
    if (photoIdx > 0 && (int32_t)(now - nextPhoto) >= 0) {
      int n = cfg.photoCount - photoIdx + 1;
      if (!postJob(JOB_PHOTO, fb, photoBase + "_" + n + ".jpg", n, cfg.photoCount)) logEvent("error", "camera", "could not save photo (out of memory)");
      photoIdx--;
      nextPhoto = now + cfg.photoIntervalMs;
    }

    esp_camera_fb_return(fb);
    if (millis() - t1 > loopMax) loopMax = millis() - t1;
    delay(aviOpen ? 5 : 40);  // idle at ~15 fps; while recording still leave time for the web page
  }
}

void cameraStart() {
  jobs = xQueueCreate(400, sizeof(Job *));
  // core 1 below the camera (4) and the microphone (5), away from Wi-Fi and the web page on core 0
  xTaskCreatePinnedToCore(writerTask, "writer", 6144, nullptr, 1, nullptr, 1);
  xTaskCreatePinnedToCore(cameraTask, "camera", 8192, nullptr, 4, nullptr, 1);
}
int cameraBacklog() { return jobsQueued; }
