# Pinout and addresses

[Polski](../pl/pinout.md) · [Wiring](wiring.md) · [Back to README](../../README.md)

All values live in `config/config.h`. Pins marked **GPIO** are ESP32-S3 pins.

| Module | Interface | Pin / address | Notes |
|---|---|---|---|
| GPS LG290P | UART1, 460800 baud | RX **GPIO4**, TX **GPIO5** | Baud rate is detected automatically |
| IMU BNO085 | UART-RVC mode, 115200 | sensor's SDA wire → **GPIO6** (ESP32 RX), RST → **GPIO7** (`IMU_RESET_PIN`), PS0 → **3.3 V** | PS0 selects RVC mode - solder it permanently. 3.3 V and GND for power. No compass is used |
| ADS1115 | I²C, `0x48` | SDA **GPIO8**, SCL **GPIO9** | channel 0 = steering angle, channel 1 = motor current |
| MCP23017 | I²C, `0x20` (same bus) | 16 outputs, pins **0-15**; before AgOpenWeb sends its pin config: Sec1-16 on pins **0-15** | The function of each pin follows the AgOpenWeb Pin Config (Machine → Pin Config); "Invert relay" inverts all of them |
| Cytron MD13S | PWM + DIR | PWM **GPIO16**, DIR **GPIO17** | Motor power does not go through the ESP32 |
| Current sensor ACS71240KEXBLT-030B3 (±30 A) | analog → ADS1115 | channel 1 | Power it from 3.3 V (not 5 V - that would damage the ADS1115 input) |
| W5500 Ethernet (optional) | SPI | CS **GPIO10**, MOSI **11**, SCK **12**, MISO **13** | See the RST note below. Enable in the panel: Settings → Ethernet |
| Steering button | input, pull-up | **GPIO15** | Pressed = connected to GND |
| Work switch | input, pull-up | **GPIO18** | Active = connected to GND |
| Status LED WS2812 | 1 wire | **GPIO38** | |

### W5500: RST is intentionally not connected

GPIO14 is left free and the driver gets `-1` instead of an RST pin in `ETH.begin()`. With RST connected the link
dropped periodically under load and showed constant transmission errors (confirmed by a test: 0 errors with RST
disconnected, constant errors with it connected). The module has its own power-on-reset circuit on the board.

### Power pins

Spread the supply across several header pins: 8 × GND, 3 × 5 V, 2 × 3.3 V were used on the original build.

### Network ports

| Port | Direction | Purpose |
|---|---|---|
| 9999 | module → AgOpenWeb | AgOpenWeb frames (PANDA, steering status, ...) |
| 8888 | AgOpenWeb → module | the module listens here (steering and machine commands, RTCM) |
| 5126 | auxiliary | auxiliary UDP port the module also listens on (`AUX_PORT`) |
| 2233 | AgOpenWeb → module | raw RTCM corrections relayed to the GPS (`RTCM_LISTEN_PORT`) |
| 80 | browser → module | web panel (`/update` is the firmware upload page) |
