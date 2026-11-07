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
  uint32_t *index = nullptr, cap = 0, frames = 0, movi = 4, maxFrame = 0, start = 0;
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
    path = p; frames = 0; movi = 4; maxFrame = 0; w = h = 0; start = millis();
    uint8_t b[AVI_HDR];
    header(b, 0, 0, 0, 0, 4, 0, 0);
    lockSD();
    file = openSD(path, FILE_WRITE);
    if (file) file.write(b, AVI_HDR);
    unlockSD();
    return (bool)file;
  }

  void add(camera_fb_t *fb) {
    if (frames >= cap) {
      uint32_t nc = cap ? cap * 2 : 512;
      uint32_t *ni = (uint32_t *)ps_realloc(index, nc * 8);
      if (!ni) return;
      index = ni; cap = nc;
    }
    if (!w) { w = fb->width; h = fb->height; }
    uint32_t len = fb->len, padded = (len + 1) & ~1UL;
    uint8_t c[8];
    memcpy(c, "00dc", 4); put32(c + 4, len);
    lockSD();
    file.write(c, 8);
    file.write(fb->buf, len);
    if (padded > len) file.write((uint8_t)0);
    unlockSD();
    index[frames * 2] = movi; index[frames * 2 + 1] = len;
    movi += 8 + padded;
    if (len > maxFrame) maxFrame = len;
    frames++;
  }

  float finish() {
    uint32_t ms = millis() - start;
    float fps = ms ? frames * 1000.0f / ms : 0;
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
  Avi avi;
  bool aviOpen = false;
  String photoBase;
  int photoIdx = 0;
  uint32_t nextPhoto = 0, lastSnap = 0, lastMotion = 0, frames = 0;
  int failures = 0;
  logEvent("camera", "camera", "task started");

  for (;;) {
    if (configDirty) { configDirty = false; applyNow(); bgValid = false; delay(200); }
    camera_fb_t *fb = esp_camera_fb_get();
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
      aviOpen = avi.begin(evidencePath(stamp() + "_" + vtag + ".avi"));
      live.videoRec = aviOpen;
      if (!aviOpen) { logEvent("error", "camera", "could not create video file"); portENTER_CRITICAL(&reqMux); videoWanted = false; portEXIT_CRITICAL(&reqMux); }
    }
    if (aviOpen && wantVideo) avi.add(fb);
    if (aviOpen && !wantVideo) {
      float fps = avi.finish();
      aviOpen = false;
      live.videoRec = false;
      logEvent("saved", "camera", "video " + avi.path.substring(avi.path.lastIndexOf('/') + 1) + " (" + String(avi.frames) + " frames, " + String(fps, 1) + " fps)");
      if (avi.path.indexOf("_telegram-") >= 0) telegramSendFile(avi.path, "221B: the moving picture you asked for");
    }

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
      lockSD();
      File f = openSD(photoBase + "_" + n + ".jpg", FILE_WRITE);
      bool ok = f;
      if (f) { f.write(fb->buf, fb->len); f.close(); }
      unlockSD();
      if (!ok) logEvent("error", "camera", "could not save photo");
      if (ok && n == 1) { String p = photoBase + "_1.jpg"; portENTER_CRITICAL(&reqMux); strlcpy(lastPhoto, p.c_str(), sizeof(lastPhoto)); portEXIT_CRITICAL(&reqMux); }
      if (--photoIdx == 0) logEvent("saved", "camera", String(cfg.photoCount) + " photo(s) " + photoBase.substring(photoBase.lastIndexOf('/') + 1) + "_*.jpg");
      nextPhoto = now + cfg.photoIntervalMs;
    }

    esp_camera_fb_return(fb);
    delay(aviOpen ? 5 : 40);  // idle at ~15 fps; while recording still leave time for the web page
  }
}

void cameraStart() { xTaskCreatePinnedToCore(cameraTask, "camera", 8192, nullptr, 4, nullptr, 1); }
