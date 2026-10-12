# First start checklist

[Polski](../pl/first-start.md) · [Getting started](getting-started.md) · [Wiring](wiring.md) · [Troubleshooting](troubleshooting.md) · [Back to README](../../README.md)

Do the steps in order and do not skip ahead: every step assumes the previous one is fine. Steps 1-3 are done with the motor and the hydraulics **disconnected**. In steps 4-5 the motor may be wired to the driver, but it must be mechanically free (disconnected from the steering, or the wheels lifted) and the emergency stop within reach.

## 1. Before power

- Pull-down resistors on the motor driver PWM and DIR inputs, so they cannot float while the module starts (see [Wiring](wiring.md)).
- A fuse in the motor supply and an emergency stop that cuts the motor power.
- Motor, hydraulic valves and relay board still disconnected.

## 2. Power and network

- The build passed in GitHub Actions and the firmware is flashed.
- The panel opens; **Diagnostics** shows WiFi (or Ethernet) connected and AgOpenWeb talking to the module.
- Set a panel PIN or password in **Settings** before the module is on a shared network.

## 3. Sensors

- GPS: fix quality and satellites look right in **Readings**; RTK corrections reach the receiver.
- IMU and angle sensor show sensible values; turning the wheel changes the angle in the right direction.
- Calibration: save the LEFT and RIGHT end points again after any change to the sensor mounting.

## 4. Safety chain on the bench

- In **Settings → Features** switch on **Motor driver** and **Current sensor** and save (both are off by default; without the current sensor there is no over-current protection). Then, with steering disengaged, run **Zero** and set the current thresholds in **Calibration**.
- Switch steering on in AgOpenWeb: the motor must stay still until steering has been seen switched off once after the restart (the AgOpenWeb screen says so).
- Motor test in **Calibration** with the motor mechanically disconnected from the steering: check the direction; if it is reversed use *Invert motor* in AgOpenWeb (or tick *Reverse motor direction (DIR pin)* in Calibration; save it with steering disengaged).
- Pull the angle sensor wire while steering is on: the motor must stop and the AgOpenWeb screen shows a warning.

## 5. Machine outputs

- Set the pin functions (sections, HydUp / HydDown, tram lines, geo-stop) and *Invert relay* in AgOpenWeb; **Diagnostics** shows the pin map and the outputs that are on.
- Check every output with a lamp or a meter before any valve or relay board is connected.

## 6. First drive

- Open ground, very low speed, one person at the emergency stop.
- Start with a low P gain and raise it in small steps; watch the target against the real angle in **Charts**.
- Test sections and the hydraulic lift in the field only after steering behaves.
