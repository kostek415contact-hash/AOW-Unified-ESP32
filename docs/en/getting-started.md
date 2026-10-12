# Getting started

[Polski](../pl/getting-started.md) · [Wiring](wiring.md) · [User guide](user-guide.md) · [Back to README](../../README.md)

> ⚠️ Read [SAFETY.md](../../SAFETY.md) first. First tests are done with the **motor disconnected** and the wheels lifted.

## 1. Build and upload

1. Install the **Arduino IDE** and the **ESP32 Arduino core 3.3.11** (the tested version; the same one the GitHub release builds use).
2. Install the libraries: **Adafruit ADS1X15**, **Adafruit MCP23X17**, **Adafruit NeoPixel**, **Adafruit BNO08x RVC** (tested: ADS1X15 2.6.2, MCP23017 2.3.2, NeoPixel 1.15.5, BNO08x RVC 1.0.2, BusIO 1.17.4).
3. Select the board **ESP32-S3** and set **PSRAM: OPI PSRAM** (required for the log buffer and the chart history).
4. *(Optional, recommended)* copy `secrets.example.h` to `secrets.h` and fill it in:

   | Field | Meaning |
   |---|---|
   | `SECRET_WIFI_SSID`, `SECRET_WIFI_PASS` | start-up WiFi network (you can also enter it later in the panel) |
   | `SECRET_AP_PASSWORD` | password of the module's own access point, min. 8 characters. Without it the module makes a **random password** on first start (see below) |
   | `SECRET_OTA_PASSWORD` | password for OTA updates (Arduino IDE and the `/update` page, user `admin`). **Write it down** - if it is lost, only a USB cable can flash new firmware |

   `secrets.h` is in `.gitignore` and never reaches git. Without it the project still compiles.
5. Open `AOW-Unified-ESP32.ino` (the other `.h` files appear as tabs) and **Upload** over USB.

Prefer not to build? Every release on GitHub contains ready binaries: `*-full-flash.bin` (first flash over USB) and
`*-ota.bin` (update through the panel's `/update` page), with `SHA256SUMS.txt`.

## 2. First start

1. With no known WiFi network the module opens its own access point **`AOG-ESP32-Setup`** (password: the one from
   `secrets.h`; without it, the random one the module printed to the **USB serial monitor**, 115200 baud, when the access
   point started - open the monitor and press the reset button to see it again).
2. Connect your phone or laptop to it and open **http://192.168.4.1**.
3. The panel offers to protect changes with a **PIN** (4-8 digits), a **password** (8-63 characters) or nothing. Viewing
   stays open; every change needs the code.
4. Go to **Settings** and enter your WiFi network (up to 4 networks; the strongest visible one is used, like a phone does).
   Press **Save network**, then **Restart now**.
5. Afterwards open the panel at **http://AOG-ESP32.local** or at the IP shown in *Diagnostics → WiFi*.

A **phone hotspot** works too: enter its name and password. The module takes address, mask and gateway by DHCP, so the
changing hotspot subnet is not a problem.

## 3. Connect to AgOpenWeb

1. Put the module and the computer/tablet running AgOpenWeb in the **same network**. On a **phone hotspot** use the
   [AgOpenWeb fork with corrected unicast](https://github.com/kostek415contact-hash/AgOpenWeb); the stock version is fine on a normal router.
2. Open the panel **Status** tab: the *AgOpenWeb* tile shows `connected` once frames arrive, with the sender's IP.
3. Check **Settings → Steering engage detection** if engaging from AgOpenWeb is not recognised (default: bit 0x01 of the
   status byte).

## 4. GPS and RTK

- The LG290P is configured automatically at start-up. **Status** shows the fix (`RTK FIX` is the goal).
- For corrections enter your **NTRIP** caster in **Settings** (host, port, mountpoint, user, password) and enable the
  built-in client; alternatively let AgOpenWeb relay RTCM to the module.

## 5. Calibrate

Follow the [user guide](user-guide.md#calibration): angle sensor first, then IMU rotation, then the current sensor zero.

## Updating later

OTA from the Arduino IDE (network port) or the `/update` page. Restart, factory reset and uploads are **refused while
steering is engaged** - disengage in AgOpenWeb first.
