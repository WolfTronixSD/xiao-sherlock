# The Chronicles

The same history is shown in the web GUI under **The Workshop**.

## 3.12.2 (2026-11-07): A false alarm in the log
- The Telegram photo was saved to the card, but the case log said "could not save" (the check ran after the file was closed).

## 3.12.1 (2026-11-07): Room to breathe
- Memory: Telegram's encryption buffers (about 40 KB per connection) now live in the PSRAM. With the bot listening for commands they sat in the internal RAM, which dropped to 0 KB and made saving files fail at times.

## 3.12.0 (2026-11-07): Pictures by telegraph
- Telegram `photo` also saves the picture to the SD card (`..._telegram-photo.jpg`).
- New commands `audio [s]` (default 5 s, up to 30) and `video [s]` (default 5 s, up to 20). From Telegram the clip is saved to the card and sent to your chat as a file; over LoRa it is only saved.
- More commands: `last` (photo of the latest alarm), `log [n]` (latest case log lines), `mute [1h]` / `unmute` (keep recording, no alerts), `arm 2h` (arm for a while, then disarm by itself), `disarm 30` (same as snooze). Durations take h or m (2h, 90m) or plain minutes.
- Telegram shows tap buttons under each reply (Status, Photo, Last, Arm, Disarm, Mute 1h, Audio, Video, Log) and a / command menu.
- Memory: ordinary memory requests of 2 KB or more now go to the 8 MB PSRAM, so the small internal RAM (Wi-Fi, camera, task stacks) no longer runs out during Telegram sends. Lowest free memory had reached 0 KB.

## 3.11.0 (2026-11-07): The librarian and the telegraph
- Evidence Locker: with thousands of files the file list took 25 s and the browser gave up ("The locker is jammed: Failed to fetch"), and the statistics came back empty. A background librarian now walks the card and keeps the newest 600 files and the statistics ready, so the page answers at once. The walk hands the SD card back every 64 files, so recording carries on, and picking the newest files no longer rescans the whole list for each file. The timeline only reads the chosen day's folder.
- New file names with the weekday, easier to read: `2026_11_06_091801_Fri__motion-Biscuit-tin.avi` in the day folder `2026_11_06`. Old names and folders (`20261106_091801_...`) still show up, count and delete as before.
- Telegram commands: switch on *Two-way: accept commands from your chat* in The Telegram Boy and send the bot status, arm, disarm, ring, quiet, snooze 30, photo, sleep or help (with or without /). Only your own chat ID is obeyed. Photo sends a picture back. Commands older than 2 minutes (sent while the camera was off) are skipped. Listening pauses when memory is low.

## 3.10.2 (2026-11-07): The locker door
- LoRa commands: just send `status`, `arm`, `snooze 30`... as the whole message, no "221B" in front needed (it still works). The Command PIN is now optional: leave it empty to accept commands without one. Ring and photo confirm before they start, and a missing PIN gets a hint back.
- Fix: the Evidence Locker statistics (and the file list) walked thousands of SD files in one go inside the web task, starving core 0's idle task, so the task watchdog restarted the board in a loop. The walk now yields every 32 entries, and the statistics answer is cached for 60 s.

## 3.10.1 (2026-11-07): Watson keeps his notes
- Fix: settings could silently fail to save (one ~3 KB NVS string needs a single page with room for all of it). After the next restart the board came back with older settings, for example the Pigeon Post off and disarmed. Settings are now an NVS blob, every save is checked, saves from different tasks no longer overlap, and a failed save shows a warning on The Scene and in the case log.
- After a watchdog restart, the restart line names what each core was running, to find the task that hung.
- Bigger version seal; the bell's Active HIGH/LOW buttons are only in Settings now; alarm log lines list "lora" when a LoRa text goes out.

## 3.10 (2026-11-07): The pigeons fly both ways
- Two-way Pigeon Post: with the radio's TX wired to the ESP RX pin, text `221B <PIN> <command>` on the Meshtastic channel. Commands: arm, disarm, status, ring [s], quiet, snooze [min], photo, sleep, help. Every command gets a short reply (replies skip the one-text-a-minute limit).
- Off by default: switch on "Two-way: accept commands over LoRa" and set a Command PIN. Wrong PINs are logged; five in a row lock commands for 10 minutes. Use a private Meshtastic channel.

## 3.9.4 (2026-11-07): Faces in the crowd
- The Irregulars list is colour coded: NEW (amber) = close and first heard under a minute ago, red = close (counts), green = marked as yours by "These are mine", grey = far away (ignored). Signal bars, new ones on top, and the count turns red at the alarm level.

## 3.9.3 (2026-11-07)
- Fix: the Armed switch in Settings kept showing what it was when the page opened. Arming with the header button and then pressing Seal and send would quietly disarm the house again. The switch now follows the button.

## 3.9.2 (2026-11-07): The Pocket Watch
- The header carries a brass pocket watch with the board's time, the date, a remark for the hour, and the firmware version as a wax seal.
- The arm button now says plainly 🔒 ARMED or 🔓 DISARMED, with "tap to disarm/arm" underneath. Disarming asks first.
- Night Watch: Off / When dark / By the clock / Always on is a row of buttons now, and the section's ON badge follows it (it used to follow the black-and-white switch). Badges update as soon as you pick.

## 3.9.1 (2026-11-07)
- Fix: switching the Irregulars on crashed the board (the Bluetooth stack ran the chip out of internal memory). Bluetooth is gone; the Irregulars now listen to Wi-Fi probe requests only, and the ~30 KB of memory Bluetooth reserved is back.

## 3.9 (2026-11-07): The Baker Street Irregulars
- The Baker Street Irregulars: counts phones, watches and trackers nearby (Bluetooth LE and Wi-Fi probe requests) and raises the alarm when more than usual show up
- The Irregulars card on The Scene with the nearby devices and a These are mine button
- Privacy: only a scrambled 4-character ID, signal and times are kept in RAM; nothing is stored or sent.

## 3.8 (2026-11-07): The Adventure of the Second Witness
- The Second Witness: optionally only alarm when two different sensors fire within a few seconds (fewer false alarms from animals and wind)
- Tripwire 4 defaults to an RCWL-0516 microwave sensor (pull-down, goes HIGH)

## 3.7.1 (2026-11-07)
- Firmware version, date and time (the board's clock) under the title on every page.
- The radar's ESP TX pin is optional: the firmware only listens, so one data wire (radar TX to D7) is enough.

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
