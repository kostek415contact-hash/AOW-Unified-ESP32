# Contributing

Thanks for your interest! This is a hobby project for a real machine, so changes are made carefully.

## Ground rules

- **One change at a time**, each small enough to be checked on hardware. Describe *what* and *why* in the commit message.
- **`loop()` must never block**: no `delay()` or waiting in the main loop. Use state machines (see
  `sensors/gps_config_fsm.h` as an example, with its host test).
- **Code, comments, log messages and translation keys are English / neutral.** Polish is only one language catalog among
  others; a contributor should not need to read Polish. (Older comments are still Polish and are being translated.)
- Put logic that does not need Arduino into **plain C++ headers** and cover it with a host test in `tests/`.
- Nothing private in the repo: no passwords, network names, logins, tokens. `secrets.h` is git-ignored.

## Building and testing

1. Firmware: Arduino IDE, ESP32 Arduino core 3.x, ESP32-S3, OPI PSRAM (see [README](README.md)). Use *Verify* before submitting.
2. Host tests (no hardware needed): `bash tools/run_tests.sh` - compiles and runs every `tests/test_*.cpp`, checks the
   panel script syntax and the translation catalogs. It must end with `WSZYSTKO OK` (Polish for "ALL OK").
   The same runs on every push in GitHub Actions.

## Adding or changing texts and languages

- Panel texts: `TL('key', {param})` in `web/ui/*.h` (the page is split into head, style, body and six script parts; `web/web_ui.h` joins them); catalogs in `web/lang/lang_<code>.h`.
- Device log / panel messages: write the **English** text in code (`LogBuffer::printf(...)` or `MSG("...")`), then run
  `python3 tools/i18n_check.py --add-log` and translate the new entries in every catalog.
- A new language is a new `web/lang/lang_<code>.h`; see `web/lang/README.md`.
- `python3 tools/i18n_check.py` must report all catalogs complete.
- `python3 tools/docs_check.py` must pass: links and anchors in every `.md`, EN/PL pairs with the same structure (headings, table rows, code blocks), and pins/ports in `docs/*/pinout.md` equal to `config/config.h`. Change a doc in one language - change its pair too.

## Pull requests

Describe the change, how you tested it (host tests and, if relevant, on which hardware), and mention any safety impact.
By contributing you agree that your contribution is licensed under the project's licence, **GPL-3.0**.

## Releases

Versions look like `1.0.3`. You set `major.minor` by hand (`FIRMWARE_MAJOR_MINOR` in `config/config.h`, plus a
`CHANGELOG.md` entry when it changes); the **last number is added automatically** by GitHub Actions.

- A push to `main` that changes firmware sources (anything except `*.md`, `LICENSE`, `tests/`, `tools/`, `.github/`)
  builds the firmware and publishes a Release `vMAJOR.MINOR.N` where N = highest existing patch + 1 (0 for a new
  major.minor), with `-ota.bin` (panel / Arduino IDE update) and `-full-flash.bin` (first flash over USB).
- Documentation-only pushes just run the tests and a compile check (binaries are in the run's Artifacts).
- Force a release any time: GitHub -> Actions -> *build-and-release* -> *Run workflow* -> tick *release*.
- A local Arduino IDE build shows `MAJOR.MINOR.dev`, i.e. not an official release.
