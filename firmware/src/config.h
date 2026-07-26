// ----------------------------------------------------------------------------
// VacController — config.h
// All pin assignments and tunable constants live here and ONLY here.
// Board: D1 Mini32-class ESP32 ("ESP32 Dev Module" profile)
// Pinout follows jimk3038/CNC_Vac_Pump notes.h convention.
// ----------------------------------------------------------------------------
#pragma once
#include <Arduino.h>

// --- Pins -------------------------------------------------------------------
// LCD pins are configured in TFT_eSPI's User_Setup.h (see README), not here:
//   SCLK=18, MOSI=23, CS=5, DC=17, RST=16, BL=21
constexpr int PIN_ENC_A      = 25;   // Encoder channel A (pullup)
constexpr int PIN_ENC_B      = 32;   // Encoder channel B (pullup)
constexpr int PIN_ENC_SW     = 27;   // Encoder pushbutton (pullup)
constexpr int PIN_VAC_SENSOR = 35;   // ADC1_CH7, via 5k/10k divider
constexpr int PIN_PUMP_RELAY = 26;   // Pump relay (active low)
constexpr int PIN_ALARM_RELAY= 33;   // Alarm relay (active low) — future use.
                                     // NOTE: repo used 21, which collides with
                                     // LCD backlight. Moved to 33.
constexpr int PIN_DEBUG_LED  = 2;    // Onboard LED heartbeat

// Relays on common dual-relay boards are active LOW.
enum Relay : uint8_t { RELAY_ON = 0, RELAY_OFF = 1 };

// --- Simulation mode (bench bring-up: LCD + encoder only) -------------------
// With SIM_VACUUM defined, the ADC is bypassed and vacuum is simulated in
// closed loop against the pump output: pump ON -> vacuum rises toward
// SIM_PUMP_MAX_V10, pump OFF -> leaks down.  Serial console (115200):
//   'f' -> toggle a simulated fixture leak the pump can't keep up with
//          (drives vacuum down through the alarm stages)
// Comment this line out for the real sensor.
#define SIM_VACUUM 1

constexpr int16_t SIM_PUMP_MAX_V10 = 330;   // vacuum the sim pump can reach (27.0)
constexpr float   SIM_PUMP_RATE    = 0.004f; // rise: dV = (max-V)*rate per 10 ms
constexpr float   SIM_LEAK_RATE    = 0.0008f;// leak: dV = -V*rate per 10 ms
constexpr float   SIM_FAULT_RATE   = 0.05f; // extra leak when 'f' fault active

// --- Units ------------------------------------------------------------------
// All vacuum values are integer inHg x 10 ("v10").  20.0 inHg == 200.

// --- Thresholds -------------------------------------------------------------
constexpr int16_t THRESH_MIN      = 5;    // 0.5 inHg
constexpr int16_t THRESH_MAX      = 295;  // 29.5 inHg
constexpr int16_t THRESH_STEP     = 5;    // 0.5 inHg per encoder detent
constexpr int16_t DEFAULT_LOW_V10  = 70;  // pump ON below 7.0 inHg
constexpr int16_t DEFAULT_HIGH_V10 = 220; // pump OFF above 22.0 inHg

// --- Sensor: NXP MPXV6115VC6U ----------------------------------------------
// Datasheet: VOUT = VS * (0.007652 * P + 0.92), P in kPa (-115..0).
// VOUT ~4.6 V at atmosphere, ~0.12 V at full vacuum (output FALLS as vacuum rises).
// Divider: 5k series / 10k to GND  ->  Vadc = VOUT * 10/15.
constexpr float SENSOR_VS_MV      = 5000.0f;   // sensor supply (measure yours; see README)
constexpr float DIVIDER_GAIN      = 15.0f / 10.0f; // Vout = Vadc * 1.5
constexpr float XFER_OFFSET       = 0.92f;
constexpr float XFER_SLOPE        = 0.007652f;     // per kPa
constexpr float KPA_TO_INHG       = 0.29530f;
constexpr int16_t VAC_V10_MAX     = 339;       // -115 kPa = 33.96 inHg (sensor limit)

// --- Filtering (repo-proven: median kills flyers, EMA smooths) --------------
constexpr int SENSOR_MEDIAN_N = 15;   // median window (raw mV samples)
constexpr int SENSOR_AVG_N    = 15;   // running-average weight
constexpr uint32_t SENSOR_PERIOD_MS = 10;

// --- Two-stage alarm ---------------------------------------------------------
// Arms only after vacuum first reaches the high (green) threshold, so it
// can't trip during initial pump-down.  Offsets are below the LOW threshold.
//   Stage 1 (warn):  vac < low - WARN_OFFSET   -> UI warning
//   Stage 2 (pause): vac < low - PAUSE_OFFSET  -> alarm relay closes
//                    (dry contact to Tormach digital I/O -> feed hold)
// Stage clears when vacuum recovers above the stage threshold + hysteresis.
constexpr int16_t ALARM_WARN_OFFSET_V10  = 20;  // 2.0 inHg below low threshold
constexpr int16_t ALARM_PAUSE_OFFSET_V10 = 40;  // 4.0 inHg below low threshold
constexpr int16_t ALARM_CLEAR_HYST_V10   = 5;   // 0.5 inHg recovery hysteresis
constexpr uint32_t ALARM_QUALIFY_MS      = 500; // condition must persist this long

// --- Settings persistence ---------------------------------------------------
constexpr uint32_t SETTINGS_SETTLE_MS = 2000; // save to NVS after 2 s of no input

// --- Display update rates ---------------------------------------------------
constexpr uint32_t LVGL_TICK_MS   = 5;
constexpr uint32_t UI_UPDATE_MS   = 30;   // needle/label refresh
