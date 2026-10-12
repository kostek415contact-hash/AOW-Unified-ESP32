# Troubleshooting

[Polski](../pl/troubleshooting.md) · [Getting started](getting-started.md) · [User guide](user-guide.md) · [Back to README](../../README.md)

Turn on the relevant switch in **Diagnostics** first - it shows that subsystem's data and writes it to the **Log**.

| Symptom | Likely cause and what to do |
|---|---|
| Panel does not open | Join the module's own access point (`AOG-ESP32-Setup`) and use `192.168.4.1`. On a network try `http://AOG-ESP32.local` or the IP from *Diagnostics → WiFi*. If the module cannot join any known network it falls back to its access point by itself |
| Module keeps losing WiFi | *Diagnostics → WiFi* shows the number of drops, the last disconnect reason and outage times. Check signal strength, add a second network (up to 4), or use Ethernet |
| Forgot the panel PIN / password | Set `PANEL_AUTH_RESET_ON_BOOT 1` in `config.h`, upload, then set a new code and set it back to `0` |
| Lost the module's access-point password | Connect USB, open a serial monitor (115200 baud) and press reset: while the module is its own access point it prints its generated password at start and repeats it every minute (not if you set your own - then use the panel over your WiFi, or reflash with a new `SECRET_AP_PASSWORD`) |
| Upload on `/update` says "login required" | No OTA password is set and the panel is protected: log in to the panel first (same browser tab), then open the update page. With `SECRET_OTA_PASSWORD` set, use user `admin` |
| Forgot the OTA password | New firmware can only be flashed over USB; set a new `SECRET_OTA_PASSWORD` |
| Restart / upload refused | Steering is engaged. Disengage it in AgOpenWeb first (a safety rule) |
| *AgOpenWeb* tile never says connected | Same network? Is another AgOpenWeb sender being followed (*Settings → AgOpenWeb senders*)? Check the broadcast address in *Settings → Network* |
| GPS has no fix / few satellites | Antenna view and cabling; *Diagnostics → GPS* shows baud rate, chipset detection and the configuration result |
| Only `RTK FLOAT`, never `FIX` | Check the NTRIP tile (streaming? correction age?). *Diagnostics → RTCM* lists message types and CRC errors. Make sure the mountpoint matches your receiver |
| NTRIP fails | The tile shows the reason: network, wrong user/password, wrong mountpoint, source-table answer. Re-enter the credentials (the password field is empty on purpose; empty = unchanged) |
| IMU shows "no data" or hangs | Check **PS0 → 3.3 V** and the wire to GPIO6. A weak USB supply caused hangs on the prototype - power the board from a proper 5 V converter. The panel can reset the sensor through RST (GPIO7) |
| Ethernet link drops under load | Leave the W5500 **RST unconnected** (see [pinout](pinout.md)) |
| Motor does not move, log says "Raw reading ... is far outside the calibrated range" (Diagnostics → Angle sensor: "Reading within the calibrated range" = NO) | The angle sensor wire is probably broken or loose, or the sensor was moved after calibration. Check the wiring; if the sensor was moved, redo the left/right calibration |
| Angle looks inverted or the range is wrong | Redo the left/right calibration and enter the real end-stop angles |
| Panel shows "Angle sensor: no reading" or "Section expander MCP23017 not responding" (log: "ADS1115 / MCP23017 stopped answering on I2C") | The chip is not answering on the I2C bus: check its 3.3 V and GND, the SDA/SCL wires and connectors (GPIO8/GPIO9). The module keeps running, probes the chip every 2 s and resumes by itself as soon as it answers - no restart needed. Until then the motor stays blocked (no angle) and the sections do not follow AgOpenWeb. On a bench without sensors this is normal |
| Motor runs the wrong way | Tick *Reverse motor direction (DIR pin)* in **Calibration** and save (steering must be disengaged) |
| Motor stops by itself | The runaway detector or the current protection tripped; **Status** and *Diagnostics* show which. Fix the cause, then press **Clear detector lock** / **Clear lock** in **Calibration**. A kick-out (PGN 250) is also sent to AgOpenWeb |
| Charts say "history unavailable" | The module has no PSRAM enabled - select **OPI PSRAM** in the Arduino IDE board options |
| Steering ignores AgOpenWeb | AgOpenWeb has not sent steering settings yet (*Readings → Steering*), or the engage bit is not recognised (*Settings → Steering engage detection*) |

Still stuck? Open an issue (the template asks for the firmware version, a short description and the relevant log lines) -
never paste passwords or tokens.
