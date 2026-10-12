# FAQ

[Polski](../pl/faq.md) · [Troubleshooting](troubleshooting.md) · [User guide](user-guide.md) · [Back to README](../../README.md)

Short answers to the questions that come up most. For symptoms and fixes see [Troubleshooting](troubleshooting.md).

### Does it work over a phone hotspot?

Yes. Enter the hotspot's name and password under Settings → Network; the module takes address, mask and gateway by DHCP, so
the changing hotspot subnet is not a problem. On a hotspot there is no usable broadcast, so frames reach AgOpenWeb by
unicast - use the [AgOpenWeb fork with corrected unicast](https://github.com/kostek415contact-hash/AgOpenWeb). On a normal
router the stock AgOpenWeb is fine.

### Do I need the internet?

No. The panel is served by the module itself, in Polish and English, with no app and no cloud. The internet is needed only
for RTK corrections (NTRIP) - and even those can arrive through AgOpenWeb instead of the module's own client.

### The Status tab says NTRIP is off, but I have corrections. Why?

When the module's own NTRIP client is off and the corrections are relayed from AgOpenWeb (raw RTCM on port 2233), Status shows
"relay from AgOpenWeb" and the chip says the corrections are flowing via AgOpenWeb. That is a normal setup.

### I enabled steering in AgOpenWeb and the motor does not move after a restart

By design. After every restart the motor stays still until steering has been seen **switched off** in AgOpenWeb at least once:
switch it off and on again. This stops the machine from starting to steer by itself after a crash or a power dip. The panel
shows the state under Diagnostics → Motor ("Steering armed").

### Do I have to resend the steering settings from AgOpenWeb after every restart?

No. The values AgOpenWeb sends (gain, PWM limits, counts per degree, ...) are kept in the module's memory and restored at
start-up. After updating from an older firmware send them once from AgOpenWeb. They are never written while steering is
engaged.

### Which AgOpenWeb settings does the module actually use?

Only some of them. The rest stay inside AgOpenWeb.

| AgOpenWeb screen | Setting | Used by the module? |
|---|---|---|
| Auto Steer | P gain, maximum and minimum PWM | **Yes.** Without them the motor gets no PWM. Kept in the module memory across restarts. |
| Auto Steer | WAS Offset, Counts Per Degree, Ackermann | Received and logged, **not used**. The angle comes from the left/right calibration in the panel (Calibration tab). |
| Auto Steer / Steer wizard | Invert motor | **Yes.** Reverses the motor direction together with the panel setting (Calibration → Motor shows the result). Applied only while steering is switched off. |
| Auto Steer / Steer wizard | Invert WAS, driver type, other hardware options | Received and shown (Diagnostics). The angle sign comes from the left/right calibration in the panel; the driver is a Cytron (PWM + DIR). |
| Auto Steer | Zero WAS, SmartWAS, counts-per-degree wizard | Do not use them: they assume the module applies the offset and counts per degree to the raw sensor, but this module calibrates locally, so the angle does not change and the offset keeps growing. Calibrate in the panel instead. |
| Auto Steer | Max Steer Angle | Stays in AgOpenWeb, never sent to the module. |
| Tool, Switches | Work switch / Steer switch, active low or high | AgOpenWeb only: it decides how to read the state the module reports. Use it if a switch works the wrong way round. |
| Machine, Module | Raise time, Lower time | **Yes**: HydUp stays on for the raise time, HydDown for the lower time, after each command. With a time of 0 the output follows the command. |
| Machine, Module | Enable | Enable off means AgOpenWeb sends no automatic lift commands; the manual Raise / Lower buttons in the panel still work as long as HydUp / HydDown is assigned to a pin. |
| Machine, Module | Invert relay | **Yes.** The relay board is active LOW: all 16 outputs are inverted and idle HIGH. Kept in the module memory. |
| Machine, Module | User 1-4 | Received and shown (Diagnostics). Nothing on the board uses them. |
| Machine, Module | Look ahead | Stays in AgOpenWeb (it decides when the lift command is sent). |
| Machine, Pin Config | Pin 1-16 assignment | **Yes.** Each of the 16 outputs can carry Sec1-16, HydUp, HydDown, TramLeft, TramRight or GeoStop. Kept in the module memory; shown in Diagnostics. Pins 17-24 have no hardware. Before AgOpenWeb sends anything: Sec1-16 on pins 1-16 (MCP23017 pins 0-15), no lift outputs. |

### Why are the charts empty after a restart?

The history is kept in the module's PSRAM and starts over after a restart. It needs OPI PSRAM enabled when building; without
it the panel says the module has no memory for history and only draws live data.

### Can I use it without the motor, just for GPS and IMU?

Yes. Every feature (motor, sections, current sensor, IMU, LED) is a switch in Settings → Features, and
Ethernet has its own in Settings → Ethernet. Leave off what you have not connected.

### Which network ports must be open?

UDP 9999 (module to AgOpenWeb), 8888 and 5126 (to the module), 2233 (raw RTCM to the module) and TCP 80 for the panel. See the
table in [Pinout and addresses](pinout.md).

### Are the screenshots real?

No. All screenshots and the wiring diagram in this repository were generated by AI from demo data, not taken from a real
machine. The panel itself looks like this.

### Is it safe to use on a machine?

It is a hobby project without any warranty and not a certified safety product. Read [SAFETY.md](../../SAFETY.md) before the
first run: keep a hardware way to take over, add pull-down resistors on the motor driver inputs, and test with the wheels off
the ground.
