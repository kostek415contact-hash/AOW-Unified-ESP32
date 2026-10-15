# Changelog

Format based on [Keep a Changelog](https://keepachangelog.com/); versions are `major.minor` from `FIRMWARE_MAJOR_MINOR` in `config/config.h` plus an automatic patch number added by CI.

## [Unreleased]

### Added
- Panel: remembers the last opened tab; a slim "Ready to drive" bar (verdict plus the first problems) under the header on every tab except Status; field mode (header button **A+**, larger controls, remembered); search box in Settings; Settings categories fold on all screens (first open, choice remembered) with Collapse all / Expand all; chart groups fold; "Recently edited" shortcuts above the Settings sections; "Copy report" button next to the diagnostic report download (Log); in field mode on a phone the tab bar scrolls sideways so the tabs can be large; two status texts ("calibration", "kickout") are now translated; Log toolbar is a single scrollable row on a phone. Checked by the browser smoke test.
- **IMU mounting calibration** (`sensors/imu_mount.h`, host-tested in `tests/test_imu_mount.cpp`): the BNO085 board may now sit at ANY fixed angle (e.g. an enclosure on the cab wall with the board tilted 45°). Heading, pitch and roll of the vehicle are rebuilt from the sensor's yaw/pitch/roll and a calibrated board->vehicle rotation; before, only a rotation around the vertical axis was corrected, so a tilted board gave wrong heading, roll and pitch (a heading error as large as the slope). Calibration in the panel (Calibration -> IMU): Point A level, Point B tilted 10-15° sideways, **Save calibration**; the sign convention of the sensor's angles is detected from the accelerometer, unclear or unsafe cases are refused with a reason, a board lying flat is saved as a single turn around the vertical axis. The card shows Roll / Pitch / Yaw raw and after the correction. Part of the settings backup (`imuMount3d`) and `/status`. Checked against an independently computed ground truth for all 8 sign conventions, a cab-wall mounting and 128 other mountings (no wrong answers); needs a check on the real vehicle.

### Changed
- Docs: all panel pictures regenerated for three screen sizes (computer, tablet, phone; EN and PL) with moving demo data so the charts show lines; the user guide has a new device-comparison section. `tools/doc_screenshots.py` makes all of them.
- Panel, tablet in portrait (800-980 px): the tab bar no longer cuts off the last tab (icons hidden at that width; the bar also scrolls sideways if still too narrow).
- Panel: cards in Readings and Calibration fold by tapping their title (chevron on the right; Diagnostics cards stay always open), with "Collapse all" / "Expand all" buttons above them; the choice is remembered in the browser. Long notes and hints in Diagnostics, Calibration and Settings fold under "Details" (warnings and status messages stay visible). Checked by the browser smoke test (fold, reload, collapse / expand all).
- Panel, Settings on a phone: every section folds by tapping its title and only the first starts open (the page was about 6,400 px tall, now about 2,600 px); a jump button opens its section. Checked by the browser smoke test at 390 px (also: no horizontal overflow).
- Panel, System card: protection trips are shown shorter ("runaway 0 • current 0 • loop 0") so the row no longer wraps on a phone; the Polish text no longer has the English word "runaway".

## [1.0.0] - 2026-10-12

First stable release: `FIRMWARE_MAJOR_MINOR` is now `1.0` (the first release of a fresh repository is `v1.0.0`; CI adds the patch number).

### Added
- SECURITY.md now states what was only true in the code: the panel is fully open until a PIN is set, the PIN is stored as plain text and travels over HTTP, UDP from AgOpenWeb is not authenticated, firmware images are unsigned, `/status` is public, NTRIP login is clear text. Panel renewed pictures now include Calibration.
- DNS-rebinding guard: POST requests whose `Host` is not an IP address, `<module>.local`, the bare module name or `localhost` are refused (`RequestGuard::hostAllowed`, host-tested).
- Chaos test (`tests/test_chaos.cpp`, 13 checks, ~4.5 M random steps): link loss, sensor loss, current kickout, frozen sensor, restart (arming reset), steering on/off and changes of gain, PWM limits, angle range, target (NaN / Inf included) thrown at the motor chain in the middle of a move, with the safety rules checked after every step; the same for the machine outputs (random pin maps, stale command, manual / automatic lift, relay polarity). Both found nothing to fix; deliberately broken copies of the code are caught.
- Documentation (EN/PL): user guide now describes the Hydraulics card, restart-proof counters, the start self-check line, AgOpenWeb screen messages, the Diagnostic report and the sectioned Settings tab. Panel pictures in `docs/img/` (Readings, Settings, EN/PL) renewed; `tools/doc_screenshots.py` regenerates them from the real panel.
- Counters that outlive restarts (`diagnostics/lifetime_stats.h`, stored in the module memory, 16 host checks): starts, abnormal restarts (crash / watchdog / power dip), motor running time and protection trips (runaway, current, stall). Shown in Diagnostics -> System and in `/status`; written only when steering is not engaged (a flash write stalls the CPU briefly), at once after a trip and otherwise every 5 minutes.
- One **self-check line** in the log 20 s after start: angle sensor, IMU, section expander, current sensor, GPS and network as OK / NO (enabled but not working) / off.
- Panel (Log tab): a **Diagnostic report** button saves one text file for a bug report - versions, the "ready to drive" verdict, the whole `/status` (password and token fields hidden), recent events and the log in the firmware's own English. Checked by the browser smoke test.
- AgOpenWeb screen message after an abnormal restart (crash, watchdog, power dip), shown once as soon as AgOpenWeb is listening; a normal power-on, OTA or software restart stays silent (`HwMessage::bootAlertText`, host-tested).
- Motor decision chain extracted into the pure function `MotorPipeline::step` (`actuators/motor_pipeline.h`: gates, end stop, P controller, runaway detector, reversal pause) and simulated on a computer with a virtual steering wheel (`tests/test_motor_pipeline.cpp`, 24 checks): healthy wheel, reversed and dead motor, frozen and lost sensor, current kickout, arming, end stops, NaN/Inf, release of the latch. Behaviour is unchanged; `MotorDriver::update` only gathers the inputs and applies the result.
- Fuzz and property tests (`tests/test_fuzz_parsers.cpp`): 100,000 NMEA sentences and 60,000 NTRIP replies (cut at random lengths, in exactly-sized buffers), 200,000 random `PwmMath` calls (|PWM| never above the limit, NaN/Inf give 0) and 2,000 random `RunawayGuard` histories (a latch never releases while steering stays on).
- The panel's "ready to drive" list now also flags a motor that is not armed after a restart (red when steering is already engaged, because it will not move) and a section expander that stopped responding; the browser smoke test checks both.
- Fuzz test of all PGN parsers (`tests/test_pgn_fuzz.cpp`, 200,000 random, truncated and corrupted frames in exactly-sized buffers) and a sanitizer mode (`SANITIZE=1 bash tools/run_tests.sh`: AddressSanitizer + UndefinedBehaviorSanitizer over the whole suite), also run in CI. Both found nothing to fix.
- Compile-time checks at the end of `config/config.h`: two functions on one pin, a reserved ESP32-S3 pin (GPIO0, 19/20, 26-32), equal UDP ports or an absurd RTCM buffer size now fail the build.
- Documentation (EN/PL): first start checklist (`docs/*/first-start.md`) - from power-up to the first drive.
- Panel smoke test now also fails when any panel row throws while `/status` is complete (before, `updRows` swallowed such errors and showed "—").
- AgOpenWeb screen message "Motor not armed - switch steering off and on" (PGN 221) when steering is requested after a restart but the motor is not armed yet (repeated every 15 s while it lasts).
- Host tests for the manual hydraulic lift (`HydControl` in `io/machine_outputs.h`: manual wins over AgOpenWeb, a restart is always automatic) and for link loss (sections, tram lines, geo-stop and automatic lift go off when PGN 239 goes stale).
- Messages on the AgOpenWeb screen (PGN 221): the module tells the operator when the motor protection trips (with the cause) and when an angle sensor, section expander or IMU that worked stops working (once, then every minute while it lasts; hardware that never worked stays silent). `connectivity/hw_message.h` + 23 host tests.
- Panel smoke test in a real browser (`tools/panel_smoke.py`, Chromium via Playwright, run by `tools/run_tests.sh` and CI): serves the real panel and language catalogs from a mock of the module with `tests/fixtures/status.json`, opens every tab and a confirmation dialog in English and Polish (with every diagnostics card switched on) and survives a sparse `/status`, and fails on JavaScript errors, failed requests, untranslated keys on screen, or a fixture that lacks a key the firmware sends in `/status`. Skipped locally when Playwright is not installed; required in CI.
- NMEA parsing (checksum, field splitting, GGA / VTG / RMC / GST fields) moved to `sensors/nmea.h` and covered by 55 host tests (`tests/test_nmea.cpp`); `GpsHandler` only keeps the state.
- Panel header: a Log in / Log out button next to the language and theme selectors, shown only while the panel is protected by a PIN or password (before, logging out was only possible in Settings).
- FAQ (EN/PL): table of which AgOpenWeb settings the module actually uses and which stay inside AgOpenWeb.
- Documentation (EN/PL): getting started, wiring and parts, pinout, user guide, troubleshooting; issue and PR templates.

- Ethernet (W5500) can be switched on in the panel (Settings → Ethernet, after a restart); the Ethernet code is always built in.
- Roll sign can be inverted in the panel (Calibration → IMU); included in the settings backup.
- PGN 238 (machine config) is received; optional timed-pulse mode for the hydraulic lift output using `raiseTime`.
- HTML identifiers of the panel (tabs, icons, themes) are English; bookmarks and the theme saved by older versions still work.
- Steering settings from AgOpenWeb (PGN 252: gainP, highPWM, minPWM, ...) are kept in the module memory and restored at start-up, so the motor works right after a restart without resending them from AgOpenWeb. Written only when they change and have settled (no flash wear while dragging a slider), never while steering is engaged.
- Each module generates its own random access-point password on first start (printed to the USB serial monitor); there is no shared default any more.
- Documentation: FAQ (EN/PL: hotspot and the AgOpenWeb fork with corrected unicast, NTRIP via AgOpenWeb, arming after a restart, ports, AI-generated screenshots) and a Charts screenshot in the user guide.
- A missing or vanished ADS1115 / MCP23017 no longer slows the main loop: failed I2C transactions are retried with growing pauses, after ten in a row the chip is declared lost and only probed every 2 s, and it resumes by itself when it answers again (no restart needed). The sections expander is also checked once per second. The panel shows "not responding" (red) and a warning when the expander is enabled but does not answer.
- `tools/docs_check.py` (run by `tools/run_tests.sh` and CI): checks links and anchors in all Markdown files, that every EN document has a PL pair with the same structure, and that pins and ports in the pinout tables match `config/config.h`.
- Angle sensor: a raw reading far outside the calibrated left/right range (a broken or loose sensor wire) is rejected at once - the motor stops, the log and the panel (Diagnostics → Angle sensor) say why. Margin: `STEERING_RAW_RANGE_MARGIN` in `config.h` (15% of the span, at least 300 ADC counts).
- Motor: the manual test pulse from the panel also stops when the wheel reaches the end of its range during the pulse; a direct reversal of the motor direction goes through a 20 ms stop (no reversing under load); the panel warns (Diagnostics → Motor) when the motor is enabled but the current sensor is off.
- Motor safety: a dead-man timer cuts the motor output if the main loop hangs for over 300 ms (before, the last PWM ran on until the 5 s watchdog reset); the motor driver pins are driven LOW as the first thing in `setup()`; after a restart the motor stays still until steering has been seen switched off in AgOpenWeb at least once (the panel shows "Steering armed" and the number of loop stalls). Host tests for the PGN parsers (`tests/test_pgn_receivers.cpp`) and for the new latches.

- Machine outputs follow AgOpenWeb (Machine → Pin Config, PGN 236): any of the 16 MCP23017 pins can be Sec1-16, HydUp, HydDown, TramLeft, TramRight or GeoStop; the map is stored in the module memory. Until AgOpenWeb sends it: Sec1-15 on pins 0-14, HydUp on pin 15 as before. `io/machine_outputs.h` + 40 host tests.
- "Invert relay" (PGN 238) makes all 16 outputs active LOW; "Invert motor" (PGN 251, only applied while steering is off) reverses the motor direction in addition to the panel setting; both are stored. User 1-4 values are shown in Diagnostics. The panel shows the received pin map, the steering configuration and the effective motor direction.
- Hydraulic lift: separate raise and lower outputs; the raise / lower times from AgOpenWeb make them timed pulses (time 0 = level).

### Changed
- Panel, Calibration: the step-by-step instructions in every card are folded under "Details" (like in Settings), so the cards are short and the buttons and fields are visible at once. Checked by the browser smoke test.
- Panel (Readings tab): Hydraulics is now a normal card in the grid (state, control, Raise / Lower / Auto buttons) instead of a separate heading and card under it that left a large empty gap.
- Panel, Diagnostics: switched-on cards now line up first, in list order, at the top; the switched-off ones follow. Switching a card on moves it to the top.
- Panel, Settings redesigned for readability: titled sections (Network, AgOpenWeb connection, Panel security and backup, RTK corrections, Hardware and timing, Diagnostic logging, Module) with jump chips on top, wider cards in a plain grid, the network Save button at the end of the network section (below the fields it saves) and long explanations folded under "Details". The browser smoke test checks the sections and the card order.
- Removed the unused `/status` fields `cmdGpsValid`, `cmdGuidanceValid`, `cmdWorking` and the matching receiver bits.
- Removed the rows "GPS valid", "Guidance valid" and "Mode" (from the AgOpenWeb steer command) from Readings and Diagnostics: AgOpenWeb sends only 0 / 1 in the status byte, so these rows always showed "no" / "transport".
- Diagnostics → Machine command: the 16 dots show the output pins (number, function from the AgOpenWeb Pin Config, lit while the output is on) instead of the section mask only.
- The panel page (`web/web_ui.h`, 2300 lines) is split into `web/ui/` parts: head, style, body and six script parts (translations/utilities/protection, data definitions, UI building, backup and settings forms, log and charts, navigation and start). The compiler joins them into the same single string in flash - the page is byte-for-byte identical; the test tools read the parts through `tools/panel_src.py`.
- Hydraulics are no longer a separate feature: the lift, tram lines and geo-stop are plain pin functions from the AgOpenWeb Pin Config. Removed the "Hydraulics" and "timed pulse" switches (Settings → Features) and the fixed lift pin 15; until AgOpenWeb sends its Pin Config pins 1-16 are Sec1-16. Diagnostics shows which outputs are on now. The manual Raise / Lower buttons need a HydUp / HydDown pin in the Pin Config.
- AgOpenWeb steering chart: the motor PWM magnitude is now sent in PGN 253 (byte 12); it was always 0.
- Panel: every action that overwrites a calibration, zeroes a sensor, clears a protection latch, moves the motor or changes a safety-relevant setting now asks first in the panel's own confirmation dialog (IMU points A/B and rotation, current sensor zero / lock / thresholds, motor direction and runaway detector, test pulses, angle range, features, thresholds, network, Ethernet, engage detection, AgOpenWeb sender). Switching off the runaway detector, reversing the motor direction or switching off the motor driver / current protection gets a stronger warning. All these buttons ignore a second tap while the first is running. The server also refuses an IMU rotation change while steering is engaged.
- Panel keyboard access: the Status tiles and the GPS card open their tab with Enter / Space and are reachable with Tab; a visible focus outline on buttons, pills, tiles, tabs and selects; dialogs are announced as dialogs, keep Tab inside, close with Esc and give the focus back to the button that opened them.

### Fixed
- Documentation: first-start order (motor may be wired in steps 4-5 but mechanically free; switch on Motor driver and Current sensor first - both are off by default), FAQ default pin map, safety notes (fuse, 10 kΩ pull-downs, 300 ms dead-man, over-current protection needs the current sensor), counter names, Features row (what needs a restart), why AgOpenWeb's Zero WAS / SmartWAS / counts-per-degree wizard do not work with this module, English-only README.
- Network and Ethernet settings are refused while steering is engaged (about 20 flash writes stall the CPU), like the other settings.
- GPS UART: a 2 kB transmit buffer, so a forwarded RTCM write (up to 1.2 kB) no longer blocks the steering loop on the 128 B FIFO (several ms at 460800 baud, tens of ms at 115200).
- `$PANDA` heading field: the GPS course is sent only while moving (>= 0.5 kn); at a standstill it is 65535 (no IMU). AgOpenWeb treats any other value as a live IMU heading and fused a stale course. `HeadingMath::courseX10`, host-tested.
- Scan reply (PGN 203): bytes 9-11 now carry the network PREFIX (first three octets of the IP, e.g. 192.168.5) as AgOpenWeb expects, not the netmask (255.255.255); AgOpenWeb showed the mask in its "Change Module Subnet" boxes. New `PgnFrame::buildScanReply`, host-tested.
- Section expander: the idle state is now written to the MCP23017 output latch BEFORE the pins become outputs. Before, on an active-low relay board every section / valve relay clicked for a few ms at each start and each chip recovery.
- Dead-man timer: a `feed()` landing just after the time was read no longer looks like a 49-day stall (false trip, stray PWM 0 pass and a bogus counter written to the module memory). Signed difference + host check.
- Manual hydraulic lift: goes back to automatic by itself when the AgOpenWeb Pin Config loses the lift pins, and the panel's "Auto" button is never disabled (before, a manual override could get stuck).
- Lifetime counters: trip saves (runaway / current / dead-man) are at most one per 20 s, so a flapping sensor cannot wear the module memory. A start is still saved at once. Host check added.
- RTCM relay: buffer raised to 1200 bytes (an RTCM3 frame is up to 1029) and a longer datagram is dropped instead of being forwarded cut in the middle of a frame.
- OTA: steering engaged during a long upload no longer ends in a restart while driving; the finished image is discarded and the module keeps running.
- Reading the WiFi status from the panel no longer refreshes the network snapshot from the web-server core (possible String corruption).
- RTCM relay on UDP 2233 accepts packets only from the module's own subnet.
- NTRIP: reply buffer limit raised from 512 B to 2 KB so long redirect headers are not cut.
- The manual hydraulic lift buttons ask for confirmation; `io/section_expander.h` includes `actuators/actuators.h` itself.
- Hydraulic lift: the output was switched on also in the AgOpenWeb state "lower" (hydLift = 1); now only "raise" (2) raises, "lower" drives HydDown when it is mapped.
- Panel: Restart, Reset, IMU reset and saving an angle end point use the panel's own confirmation window instead of the browser's `confirm()`. Browsers add "Don't allow this page to prompt you again" to `confirm()`; once ticked, every such button silently did nothing. Escape or a tap outside the window cancels; the default focus is on Cancel.
- Panel: a second tap on the same action while it is still being sent is ignored and buttons are greyed out while their action runs; network failures show "no connection to the module" instead of raw browser text such as "Failed to fetch".
- Panel: the hydraulic lift buttons are disabled when the lift is switched off in Settings, show "Sent: ..." after a tap, and the module answers 409 to a manual raise/lower while the lift is off (before, it set an override that nothing cleared).
- Panel: saving a LEFT / RIGHT angle end point asks for confirmation.
- Panel: the "no connection" banner appears after 2 failed polls (was 3) and the data is dimmed while offline, so stale values are not mistaken for current ones.
- Panel: the "Restarting..." screen disappears by itself after 60 s when the module never went down (the request did not arrive); before it could stay for ever.
- Panel: returning to the tab no longer downloads the whole hour of history (about 137 KB) again within 30 s - it blocked `/status` on a slow link; the log poll no longer starts a second parallel chain.
- NMEA checksum check is strict: exactly two hex digits after the `*`. Before, text such as `*ZZ` was read as checksum 00 and a line whose real checksum is 00 passed with garbage after the asterisk.
- The firmware update page has a cleaner layout: a styled file chooser with the file name, and the login box replaces the upload form (instead of appearing under it) when a login is needed; a .bin file can be dragged onto the dashed drop area.
- The firmware update page (`/update`) checks the login BEFORE sending the file and shows a PIN/password box at once when the panel is protected and this browser tab has not logged in yet; after logging in the upload goes on. Before, the whole file was sent first and then it only said "login required" with nothing to log in with.
- The AgOpenWeb sender tile in Settings no longer flashes "selected but silent" / "last frame has not arrived yet" for one refresh: a frame arriving while the panel was reading the clock gave a negative age (race between the web task and the main loop).
- The motor stays blocked until BOTH angle end points (LEFT and RIGHT) have been saved at least once. Before, a sensor that was never calibrated (fresh flash, factory reset) fell back to the default 0..32767 mapping, which counted as a valid calibration.
- Saving motor, current-protection, steering-range, feature, threshold, sender, engage-mode or angle-calibration settings, and clearing the runaway / over-current latches, is refused (409) while steering is engaged in AgOpenWeb. Before, only restart, factory reset and OTA were.
- The runaway and over-current latches are released only after a fresh frame confirms that steering is OFF in AgOpenWeb for 1 s. Before, a link loss of 1.5 s or more released them while AgOpenWeb was still engaged, and the motor restarted on the same fault when the link came back.
- Switching the motor feature off and on again in the panel now needs a fresh frame with steering OFF before the motor can drive (the arming is forgotten while the feature is off).
- Control frames (PGN 254 / 239 / 238 / 252) whose checksum cannot be confirmed are rejected, including frames with the CRC byte missing. PGN 238 (machine config) is now on the list.
- A quote, backslash or control character in an SSID or NTRIP text no longer breaks `/status` (the panel showed "no connection" for good); these texts are escaped.
- `/update` (firmware upload) is no longer open to everyone on the network when no OTA password is set: a panel protected by a PIN/password now requires its login there too (the update page sends it automatically from the same browser tab).
- A page from another site can no longer make your browser change settings, restart or reset the module while the panel has no PIN (cross-site requests are refused by their `Origin` header).
- NTRIP: a stream that looks open but delivers no RTCM for 20 s (a cut data link) is closed and reconnected; saving new NTRIP data reconnects at once instead of waiting out the old pause; switching the client off in the panel now closes an open connection.
- Network form: addresses that parse but cannot work are refused - module address 0.x / 127.x / 224+ or a network/broadcast address, a gateway outside the module's subnet or equal to it, an AgOpenWeb address of 0.x / 127.x / 224+.
- While the module is its own access point with the generated password, the password is repeated on the USB serial monitor every minute (before: once at start only), so a person who opens the monitor later can still join.
- The GPS baud-rate retry no longer blocks the main loop for 150 ms every 15 s (it is a small state machine, `sensors/gps_baud_retry.h`); steering frames and heartbeats are no longer delayed by it.
- Position (PANDA) goes out as live only when the last fix is younger than 5 GPS periods (0.5-2 s, `sensors/live_window.h`), and the IMU counts as live for 500 ms (was 2 s for both); the panel and LED keep the 2 s window.
- The GPS-status heartbeat (PGN 218) was sent every 200 ms instead of every 100 ms because slot 24 was taken by AutoSteer; it now uses slots 5 and 25 (`connectivity/slot_schedule.h`).
- Incoming UDP is drained (up to 16 datagrams per socket per pass) instead of one datagram per pass; the unconditional 1 ms `delay(1)` after sends is kept only for the W5500, and bursts of Hello requests get at most one set of replies per 50 ms.
- Release builds now use the same ESP32 core (3.3.11), library versions and board options as the author's Arduino IDE build; the previous binaries (core 3.2.0, newest libraries) froze on the device when AgOpenWeb was connected, while the IDE build did not.
- Status: when the own NTRIP client is off but corrections arrive relayed from AgOpenWeb, the NTRIP tile and the
  "RTK corrections" chip no longer say "disabled / NTRIP off".
- Release builds show the real build time (written by CI) instead of a possibly frozen compiler `__DATE__`.

## [0.9.0] - 2026-10-09

First public release candidate. Running on the author's machine (RTK FLOAT/FIX in the field confirmed).

### Added
- Autosteer, sections and hydraulics module for AgOpenWeb on ESP32-S3 (PGN handling, `$PANDA` output, steering PID/P regulator).
- Web panel (Status, Readout, Charts, Diagnostics, Calibration, Settings, Log) with module-side 24 h history, events on charts,
  readiness card, settings backup (export/import), optional PIN/password protection.
- Polish and English for the whole panel, the device log, firmware messages and the `/update` page; neutral keys and
  `tools/i18n_check.py` enforcing completeness.
- Built-in NTRIP client (VRS) with RTCM diagnostics and RTCM relay from AgOpenWeb.
- GPS: autobaud and retry after a late start; **non-blocking configuration state machine** that writes to the receiver
  (and saves it) only when the NMEA stream differs from the target; GST accuracy, per-satellite C/N0, RTK correction age.
- WiFi: up to 4 known networks, strongest-network failover, own access-point fallback, DHCP everywhere, phone-hotspot
  friendly addressing, mDNS name (`AOG-ESP32.local`), **outage-duration measurement** and optional fast reconnect.
- IMU BNO085 (UART-RVC) with hardware reset; steering angle sensor; motor current sensor with over-current protection,
  runaway detector and kick-out to AgOpenWeb.
- OTA updates (Arduino IDE and `/update`), refused while steering is engaged.
- Diagnostics: CPU profile per step, watchdog, free-RAM **leak trend**, PANDA gap trackers, persistent last-restart events.
- Host-side tests (`tools/run_tests.sh`, 30+ suites) and GitHub Actions workflow.

### Known limitations
- Fast WiFi reconnect (`WIFI_FAST_CONNECT`) and the GPS configuration state machine are new and need more field hours.
- Current sensor: wiring, zero calibration and readout confirmed; over-current protection not yet tested under motor load.
- Ethernet (W5500) is optional and currently untested on hardware.
- The motor path is implemented but has not driven a real steering motor yet.
- Most older source comments are Polish.
