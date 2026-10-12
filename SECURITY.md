# Security policy

## Reporting a vulnerability or a safety problem

Please **do not open a public issue** for a security or safety problem (for example: a way to bypass the panel
protection, to make the motor run without a valid command, or to read stored passwords).

Report it privately using GitHub's **"Report a vulnerability"** button (Security tab -> Advisories) on this repository.
Please include the firmware version (panel -> Status; a local build shows `x.y.dev`), what you did, what
you expected and what happened. You should get a reply within about a week; this is a hobby project maintained by one
person, so there is no guaranteed response time or bounty.

## Supported versions

Only the latest release receives fixes.

## Things worth knowing

- The device is meant for a **trusted local network** (a phone hotspot or your own router). Do not expose it to the internet.
- Panel protection (PIN or password) guards every change; viewing is open. Set one on first start. **Until a PIN or password is set (or after you decline it), the panel is fully open**: anyone on the network can change settings, run the motor test, switch the lift and (without an OTA password) upload firmware, and the first person to set a code claims the panel. Set it before the module joins a shared network.
- The PIN / password is stored in the module's flash memory as plain text (not hashed) and is sent to the module over plain HTTP; a login token lasts 12 hours and there are 4 token slots. After 5 wrong codes the login is locked for 30 s, doubling up to 5 minutes (the lock is global). Anyone who can read the module's flash (physical access) or sniff an unencrypted network can read the code, so use a WPA2 network or the module's own access point.
- **AgOpenWeb traffic on UDP is not authenticated.** The module follows the first AgOpenWeb sender it hears and switches to another one after 5 s of silence; the frame checksum is a plain sum, not a signature. Anyone on the same network can therefore send steering, section and hydraulic commands (and machine pin / steering settings) once the real sender has been quiet. Pin the sender in the panel (Settings -> AgOpenWeb connection), keep the network closed (WPA2, no guests) and never use an open WiFi. The motor still needs steering to have been seen switched off once after each restart.
- There is no shared default access-point password: without `SECRET_AP_PASSWORD` in `secrets.h` the module makes a random
  one on first start, keeps it and prints it to the USB serial monitor when the access point starts. Change it in the
  panel (Settings -> Network) or in `secrets.h`.
- Firmware upload over `/update`: with `SECRET_OTA_PASSWORD` set it asks for that login (user `admin`); without it, a panel protected by a PIN/password requires the same login as for settings. Only a panel left unprotected on purpose lets `/update` through. The Arduino IDE port (ArduinoOTA) is protected only by `SECRET_OTA_PASSWORD`, so set it.
- Requests sent to the panel by a page from another site (cross-site request forgery) are refused even when the panel has no PIN, and so are requests whose `Host` is not an IP address, `AOG-ESP32.local`, the bare module name or `localhost` (DNS-rebinding guard). Read-only pages (`/status`, `/log`, `/events`) are open to anyone on the network: they show the network names, the NTRIP host, mountpoint and user name, addresses and the GPS position - never passwords.
- Firmware images are **not signed** and there is no secure boot, flash encryption or automatic rollback: whoever may upload (see above) can replace the firmware. Set `SECRET_OTA_PASSWORD`.
- NTRIP credentials are sent in clear text (HTTP Basic over plain TCP, as the protocol does) unless your caster offers something else.
- `/status` never returns passwords. `secrets.h` is excluded from git; never commit it.
- See also [SAFETY.md](SAFETY.md).
