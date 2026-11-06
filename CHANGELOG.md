# The Chronicles

The same history is shown in the web GUI under **The Workshop**.

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
