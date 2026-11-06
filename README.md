# xiao-sherlock: 221B Baker Street security recorder

A security recorder for the **Seeed XIAO ESP32S3 Sense** that watches, listens and keeps a case log, with a web GUI in a Sherlock Holmes theme.

## What it does

| Clue | Sensor | What happens |
|---|---|---|
| Noise above a threshold | built-in PDM microphone | WAV clip (with 1 s pre-roll), optional photos / video |
| Movement in a "place of interest" | camera, motion zones drawn on the live picture | photos, optional AVI video / audio |
| Door, PIR, button | up to 4 GPIO "tripwires" | photos, video, audio (each selectable) |
| Someone approaching | HLK-LD2450 24 GHz radar (up to 3 targets, shown on a radar plot) | photos, video, audio |
| Something close | HC-SR04 ultrasonic | photos, video, audio |

Everything is saved to the microSD card in `/sec/`:

- `YYYYMMDD_HHMMSS_<source>-<name>_N.jpg`: photos
- `..._<source>-<name>.avi`: MJPEG video (plays in VLC / Windows Media Player)
- `..._<source>-<name>.wav`: 16 kHz mono audio
- `caselog.csv`: every event with time, source, pin and level, mic dB, motion %, radar targets and distance

Every setting is changed in the web GUI and stored on the device: Wi-Fi, web password, pins, thresholds, zones, actions, resolution and time zone. Firmware updates can be uploaded over Wi-Fi from the Workshop tab.

## Hardware

- Seeed XIAO ESP32S3 Sense (camera + mic + SD expansion board), FAT32 microSD card
- Free pins for sensors: **D0-D7** (GPIO 1-6, 43, 44). D8-D10 and GPIO21 are used by the SD card.

| Sensor | Wiring |
|---|---|
| Door reed switch | pin - switch - GND. Input mode *Pull-up*, trigger *goes HIGH* (door opens) |
| PIR (HC-SR501, AM312) | OUT - pin, VCC - 5V/3V3, GND. *No pull*, *goes HIGH* |
| HLK-LD2450 | radar TX - ESP RX pin, radar RX - ESP TX pin (defaults D7/D6), 5V, GND. 256000 baud |
| HC-SR04 | TRIG - pin, ECHO - pin **through a 5V to 3.3V divider** (e.g. 1k/2k), VCC 5V |

Pins are checked for clashes. A sensor whose pin is already taken is disabled and a warning is shown.

## Build

Arduino IDE 2 or arduino-cli with **esp32 core 3.x** and the **ArduinoJson 7** library.

- Board: `XIAO_ESP32S3`
- PSRAM: `OPI PSRAM`
- Partition scheme: `Default with spiffs (3MB APP/1.5MB SPIFFS)` (keeps OTA working)
- USB CDC On Boot: Enabled

```bash
arduino-cli compile -u -p COM30 -b "esp32:esp32:XIAO_ESP32S3:PSRAM=opi,PartitionScheme=default_8MB,CDCOnBoot=default" SecurityCam
```

## First start

1. Optional: copy `secrets.example.h` to `secrets.h` and enter your 2.4 GHz Wi-Fi. `secrets.h` is ignored by git.
2. Without it, the board opens a hotspot **Baker-Street-221B**. Join it and open http://192.168.4.1.
3. Under **Instructions to Watson -> Telegraph Office**, set Wi-Fi (and a web password if you like; user name `holmes`), then restart.
4. Afterwards: `http://baker-street.local` or the IP address printed on the serial monitor.

## Adding a sensor (it's meant to be extended)

1. `app.h`: add a `Source` entry and its settings to `Config`.
2. `config.cpp`: describe the settings in `buildFields()`. The web form appears automatically.
3. `sensors.cpp`: claim the pins in `setupPins()`, poll it in `sensorTask()`, and call `raiseEvent(SRC_..., "who", "what happened")`.
4. Optional: a line of commentary in `REMARKS` in `web_page.h`.

## Files

| File | Purpose |
|---|---|
| `SecurityCam.ino` | start-up, Wi-Fi / hotspot |
| `app.h` | shared types and settings |
| `config.cpp` | settings table, JSON save / load, web schema |
| `core.cpp` | SD card, clock, case log, event dispatcher (cooldown, actions) |
| `camera_task.cpp` | camera owner: photos, AVI recording, motion zones, live snapshot |
| `audio_task.cpp` | microphone level, sound trigger, WAV clips |
| `sensors.cpp` | GPIO tripwires, HC-SR04, LD2450 |
| `web.cpp` / `web_page.h` | JSON API and the 221B web GUI |

## Ideas for later

Telegram/e-mail alerts, MQTT / Home Assistant, a siren or LED output, arming on a schedule, deleting the oldest files when the card is full, an IR LED for night vision.
