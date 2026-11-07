# The Chronicles

The same history is shown in the web GUI under **The Workshop**.

## 3.7 (2026-11-07): The Tidy Study
- Cleaner settings: ON/OFF switches, HIGH/LOW buttons instead of drop-downs, and the five actions as one row of chips
- Alarm bell: ON/OFF switch and Active HIGH / Active LOW buttons, also on The Scene (applied at once)
- Sensors row on The Scene: switch each sensor on or off, or all at once (All back on restores the ones you had)

## 3.6.1 (2026-11-07)
- Fix: a missing camera filled the case log with "camera restart failed" twice a second; it now retries every 30 s and logs rarely.

## 3.6 (2026-11-07): The Problem of the Vanishing Card
- Radar trigger distance slider on The Scene, saved at once without restarting the radar
- Fix: the piezo tone could take the camera clock (LEDC timer 0) and stop the pictures
- The camera restarts itself if no pictures arrive, and a missing SD card is retried every minute
- Restarts say which watchdog fired; a normal software restart is no longer shown in red

## 3.5 (2026-11-06): The Adventure of the Singing Piezo
- Alarm bell: "Piezo wail" and "Piezo beeps" sounds drive a passive piezo with a tone (LEDC), so it sounds instead of clicking.
- Tripwire debounce is adjustable (Baker Street HQ, default 50 ms) and the pins are read every 5 ms.
- The Scene polls the status twice a second, skipping a poll while the last one is still loading.

## 3.4 (2026-11-06): The Case of the Silent Bell
- Fix: an alarm bell (or IR lamp) on D1 did nothing, because the camp wake pin (default D1) was set to an input at boot even with camp mode off
- Fix: tripwires showed "?" after settings were saved, until the pin changed
- Warning when the camp wake pin is also the alarm bell or IR lamp pin

## 3.3 (2026-11-06): The Page That Would Not Starve
- The web page and case log run in their own task on core 0, so the page stays responsive while the camera, microphone and sensors on core 1 are busy recording

## 3.2 (2026-11-06): The Return of the Wireless
- On the hotspot, the house retries your Wi-Fi once a minute and switches back by itself (skipped while a phone is on the hotspot)
- Firmware upload refuses the 8 MB full USB image and stops at the first failed write instead of hanging the page

## 3.1 (2026-11-06): A Study in Scarlet (and Green, and Blue)
- Instructions to Watson: coloured tiles with big icons to jump to each section; a green dot shows which are switched on
- Each section has its own colour, big icon and ON/off badge, opens and closes with a tap, and has its own Seal and send button
- A back-to-the-tiles button in the corner

## 3.0 (2026-11-06): The Adventure of the Sleeping Detective
- Camp mode: deep sleep on a battery; the PIR (wake pin) wakes it, it records photos/video/audio to SD with no Wi-Fi, then sleeps again once quiet
- Press BOOT to wake it with the hotspot Baker-Street-221B (or home Wi-Fi if in range); it stays awake while someone is looking at the page
- The Pigeon Post: alerts as Meshtastic LoRa texts through a radio on a UART (serial module, TEXTMSG mode), with a test button
- Optional LoRa heartbeat with battery voltage and free card space; battery voltage pin with divider
- The visiting phone sets the clock when there is no internet; "Send Holmes to sleep now" button
- Telegram is skipped when there is no Wi-Fi

## 2.9 (2026-11-06): The Hound in the Dark
- The Night Watch: black and white picture plus an IR lamp output (any free pin) when it gets dark, by the clock, or always
- Light level shown on The Scene, written in the case log, with separate night and day levels so the lamp does not flicker
- The Alarm Bell: siren/buzzer/LED output, steady, slow beep or fast blink, rung by any sensor with "Ring the alarm bell" ticked
- Ring or silence it from The Scene; disarming silences it
- The Diogenes Club: MQTT reports (state, events, clue) and commands (armed, siren, test), with Home Assistant auto discovery
- MQTT status in the Doctor's Report

## 2.8 (2026-11-05): The Doctor's Report
- Health panel on The Scene: Wi-Fi signal, chip temperature, uptime, memory (now, lowest, largest piece), PSRAM, card use and restarts
- Each line has a lamp: green good, amber watch, red trouble
- Restarts are counted and the reason (power on, crash, watchdog, brownout...) is shown and written to the case log

## 2.7 (2026-11-05): The Tidy Lodger
- Make room: when the card is fuller than 90% (adjustable), the oldest day is deleted, then the next, until there is room
- Today and the case log are never touched; each clean-up is noted in the case log

## 2.6 (2026-11-05): Mrs Hudson's Timetable
- Arm and disarm on a daily schedule (acts only at that minute, so the button still wins)
- Watches for your phone on the Wi-Fi: arms when it leaves, disarms when it comes back
- The morning disarm is skipped while the phone is out; every change is noted in the case log

## 2.5 (2026-11-05): The Adventure of the Red Line
- Listening Post: drag the red threshold line on the meter; it is saved at once
- Settings retry loading and "Seal and send" stays disabled until they have loaded (no more overwriting with empty settings)
- The time zone is applied at boot, before the first file is saved

## 2.4 (2026-11-05): The Statistician's Notebook
- Evidence Locker statistics: totals, clues per day (last 30 days), by hour of day and by sensor
- The Timeline: one day as a 24-hour strip of photos, videos and recordings; tap a mark to open it
- Fix: files saved right after a restart were named in UTC instead of local time

## 2.3 (2026-11-05): The Sign of the Swift Lens
- Low-latency live view: the next picture is requested as soon as the last one arrives (fps shown)
- Touch zone editor: draw, move and resize places of interest with a finger on iPhone
- Sliders for sensitivity and number settings
- Fix: network timeouts were 1000x too short (file downloads cut off, Telegram connect could fail)
- Fix: the Evidence Locker list no longer opens every file (thousands of files froze the web page); shows the newest 600 and the total
- Fix: the Telegram sender can no longer trip the watchdog and reboot the board
- Fix: video recording no longer starves the web page; a web watchdog restarts the board if the page is stuck for 45 s and logs where
- Fix: writing the case log never waits for a busy SD card (the web page stalled while photos were saved); SD bus at 20 MHz
- New evidence is stored in one folder per day (/sec/YYYYMMDD/); a folder with thousands of files made every save take seconds

## 2.2 (2026-11-05): The Adventure of the Pocket Telegraph
- Telegram alerts with a photo, switchable per sensor, with a rate limit and a "Find my chat ID" button
- The Kinetoscope: play `.avi` clips in the browser (iPhone too)
- Burn a whole day or everything in the Evidence Locker
- Icons and taglines for every settings section
- The Wi-Fi field shows the network currently in use
- Version history in the GUI

## 2.1 (2026-11-05): The Case of the Frozen Safari
- Files are sent in small pieces with a timeout, so a paused iPhone download can't freeze the device
- HTTP range requests, so audio and video play in Safari
- The SD card is retried and remounted automatically

## 2.0 (2026-11-05): A Study in Sensors
- 221B Baker Street web GUI
- Motion zones drawn on the live picture
- AVI video clips
- Up to 4 GPIO tripwires, HLK-LD2450 radar, HC-SR04 ultrasonic
- Detailed case log (`caselog.csv`)
- All settings in the web page, OTA firmware upload
- No secrets in the source code

## 1.1: The Quiet Microphone
- Recording volume boost

## 1.0: The First Case
- Sound trigger with WAV clips and photos, one GPIO trigger, simple settings page
