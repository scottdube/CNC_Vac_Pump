# VacController — CNC Vacuum Pump Controller

Closed-loop vacuum controller for CNC work holding. D1 Mini32-class ESP32,
GC9A01 1.28" round LCD, NXP MPXV6115VC6U sensor, rotary encoder, relay output.

Requirements doc: Claude project "Vacuum Pump controller" → `claude/requirements.md`
UI assets: SquareLine-generated files from [jimk3038/CNC_Vac_Pump](https://github.com/jimk3038/CNC_Vac_Pump)
(no license file in that repo — personal/shop use).

**Compile-verified 2026-07-25:** esp32 core 3.3.11, LVGL 8.3.11, TFT_eSPI 2.5.43
→ 55% flash / 28% RAM on ESP32 Dev Module profile. Not yet run on hardware.

---

## PlatformIO (this project's format — recommended)

Open this folder in VSCode (File → Open Folder); PlatformIO picks up
`platformio.ini` automatically. Libraries (lvgl 8.3.11, TFT_eSPI 2.5.43) and
the ESP32 platform download on first build — no manual installs.

- **Build:** PlatformIO toolbar ✓ (or `pio run`)
- **Upload:** → arrow (or `pio run -t upload`) with the board on USB.
  If the port isn't auto-detected, add `upload_port = ...` to platformio.ini.
- **Serial monitor:** plug icon (or `pio device monitor`) — 115200, already set.
  This is where `Settings saved` prints and where you send `f` for the
  simulated leak.

All TFT_eSPI display pins are build flags in `platformio.ini` — never edit
the library's User_Setup.h in this workflow. `lv_conf.h` lives in `include/`.
Everything else (pins, thresholds, SIM_VACUUM) is still `src/config.h`.

USB-C clone note: if upload fails to connect, hold the board's BOOT/IO0
button (if present) when esptool retries, and expect the CH340/CP210x USB
serial driver to be needed on macOS if the port doesn't appear at all.

## Alternative: Arduino IDE setup (not needed if using PlatformIO)

1. **Boards Manager:** install "esp32 by Espressif". Board: **ESP32 Dev Module**
   (defaults are fine; repo-recommended settings in its notes.h also work).
2. **Library Manager:** install exactly
   - `lvgl` **8.3.11**
   - `TFT_eSPI` **2.5.43**
3. **lv_conf.h** — copy this sketch's `lv_conf.h` into your Arduino libraries
   folder, *next to* (not inside) the `lvgl` folder:
   `~/Documents/Arduino/libraries/lv_conf.h`
4. **TFT_eSPI display config** — in `~/Documents/Arduino/libraries/TFT_eSPI/`:
   - `User_Setup_Select.h`: comment out `#include <User_Setup.h>`,
     uncomment `#include <User_Setups/Setup46_GC9A01_ESP32.h>`
   - `User_Setups/Setup46_GC9A01_ESP32.h`: set pins to
     ```
     #define TFT_MISO 19
     #define TFT_MOSI 23
     #define TFT_SCLK 18
     #define TFT_CS    5
     #define TFT_DC   17
     #define TFT_RST  16
     #define TFT_BL   21
     #define TFT_BACKLIGHT_ON HIGH
     ```
   Note: TFT_eSPI library updates overwrite these edits — re-apply after any
   library upgrade (or don't upgrade; versions are pinned for a reason).

## Wiring

| Signal | ESP32 GPIO | Notes |
|---|---|---|
| LCD SCLK | 18 | |
| LCD MOSI (Din) | 23 | |
| LCD CS | 5 | |
| LCD DC | 17 | |
| LCD RST | 16 | |
| LCD BL | 21 | |
| Encoder A | 25 | internal pullup |
| Encoder B | 32 | internal pullup |
| Encoder SW | 27 | internal pullup |
| Vacuum sensor out | 35 | **via divider**: sensor out → 5k → GPIO35 → 10k → GND. 100 nF from GPIO35 to GND recommended. |
| Pump relay IN | 26 | active low |
| Alarm relay IN | 33 | active low, unused in v1 (moved from repo's 21 — that collides with LCD BL) |

Sensor: 5.0 V supply. Output is **inverted** — ~4.6 V at atmosphere,
~0.12 V at full vacuum.

## Operation

- Boot: spinner until sensor filter warms up (~0.3 s); pump held OFF until then.
- Gauge: needle + digital inHg readout. Red LED icon = pump running.
- Encoder: **rotate** = move active bug 0.5 inHg/detent. **Push** = toggle
  red (low/pump-ON) ↔ green (high/pump-OFF) bug. Bugs push each other
  (high always ≥ low + 0.5).
- Thresholds save to NVS 2 s after the last adjustment and restore on boot.
  Defaults: 7.0 / 22.0 inHg.
- Control: vac ≤ low → pump ON; vac ≥ high → pump OFF; hold in between.

## Two-stage alarm (feed hold)

Arms only after vacuum first reaches the high (green) threshold — initial
pump-down can't trip it. Both stages are qualified for 500 ms to reject
transients, and clear with 0.5 inHg of recovery hysteresis.

| Stage | Trip level | Action |
|---|---|---|
| 1 — warn | vac < low − 2.0 inHg | digital readout turns amber |
| 2 — pause | vac < low − 4.0 inHg | readout flashes red + **alarm relay closes** (GPIO 33, active low) |

Wire the alarm relay's dry contact to the Tormach digital I/O board input
configured for feed hold. The relay stays closed while the condition holds;
PathPilot latches the pause on its side, so vacuum recovery here does not
resume the program — the operator does, after fixing the problem.

Offsets, qualify time, and hysteresis: `ALARM_*` constants in `src/config.h`.

## Bench bring-up (LCD + encoder only — no sensor, no relays)

This build ships with `SIM_VACUUM` **enabled** in `src/config.h`: the ADC is
bypassed and vacuum is simulated in closed loop against the virtual pump.
Nothing needs to be wired except the display and encoder.

What you should see after flashing:

1. Spinner for ~0.5 s, then the gauge with needle at ~0 and thresholds at the
   NVS/default bugs (7.0 / 22.0).
2. Vacuum "pumps down": needle climbs toward the green bug (pump indicator on),
   pump cuts out at the high bug, vacuum slowly leaks back to the red bug,
   pump kicks in again — continuous hysteresis cycling.
3. Rotate encoder → active bug moves in 0.5 steps; push → switches bugs;
   cycling adapts to the new thresholds. 2 s after the last click the serial
   monitor prints `Settings saved` (NVS persistence check — power cycle and
   the bugs should come back where you left them).
4. Serial monitor (115200): send `f` to simulate a fixture leak the pump
   can't keep up with — readout turns amber at low−2.0 (stage 1), then
   flashing red at low−4.0 (stage 2, alarm relay pin asserts). Send `f`
   again to clear; vacuum recovers and stages clear in reverse.

When the real sensor is wired: comment out `#define SIM_VACUUM 1` in
`src/config.h` and reflash. No other changes.

## First-flash checklist (things code can't verify)

1. **Needle direction/zero** — angle maps in `src/display.cpp` are copied from
   the repo and assume its gauge artwork. If the needle points wrong at
   atmosphere (should read ~0), adjust `needleAngle()`.
2. **Encoder direction** — if rotation feels backwards, swap PIN_ENC_A/B in
   `src/config.h`.
3. **Relay polarity** — code assumes active-low relay board. If your relay is
   active-high, flip the `Relay` enum in `src/config.h`.
4. **Sensor supply** — accuracy is ratiometric to the 5 V rail. Measure the
   actual rail and set `SENSOR_VS_MV` in `src/config.h` (e.g. 4870.0f).
5. **Divider values** — if you use different resistors than 5k/10k, update
   `DIVIDER_GAIN`.

## Module map

```
VacController.ino   wiring + loop scheduling only
src/config.h        every pin and constant
src/sensor.*        ADC (calibrated mV) -> median-15 -> avg-15 -> inHg x 10
src/control.*       hysteresis pump state machine (independent of GUI)
src/input.*         encoder FreeRTOS task on core 0
src/settings.*      NVS thresholds, settle-delay save
src/display.*       LVGL init + gauge updates
src/ui_lvgl/        SquareLine-generated UI (from jimk3038 repo) — don't
                    hand-edit; regenerate from SquareLine if artwork changes
```

Rule that keeps this project healthy: **UI changes never touch control or
input code.** That's the regression pattern that killed the previous attempt.
