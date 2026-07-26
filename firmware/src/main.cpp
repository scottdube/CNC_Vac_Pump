// ----------------------------------------------------------------------------
// VacController — closed-loop vacuum pump controller for CNC work holding
//
// Board:    D1 Mini32-class ESP32 (Arduino IDE profile: "ESP32 Dev Module")
// Display:  1.28" round GC9A01 240x240 (SPI) via TFT_eSPI 2.5.43 + LVGL 8.3.11
// Sensor:   NXP MPXV6115VC6U (analog, 5k/10k divider -> GPIO35)
// Input:    quadrature encoder + pushbutton (FreeRTOS task, core 0)
// Output:   pump relay, hysteresis control
//
// Architecture: modules in src/ own their state; this file only wires them.
//   src/config.h    pins + constants (edit here, nowhere else)
//   src/sensor      ADC -> median+avg filter -> inHg x 10
//   src/control     hysteresis pump state machine (runs even if GUI stalls)
//   src/input       encoder task -> settings
//   src/settings    NVS-persisted thresholds (2 s settle, then save)
//   src/display     LVGL UI (repo's SquareLine assets)
//
// See README.md for one-time library setup (User_Setup.h, lv_conf.h).
// Requirements doc: project "Vacuum Pump controller" / claude/requirements.md
// ----------------------------------------------------------------------------
#include <Arduino.h>
#include <lvgl.h>           // only for the version printout below
#include "config.h"
#include "sensor.h"
#include "control.h"
#include "input.h"
#include "settings.h"
#include "display.h"

static uint32_t s_ctrlMs, s_heartMs;

void setup() {
    Serial.begin(115200);
    Serial.println("\nVacController boot");

    pinMode(PIN_DEBUG_LED, OUTPUT);

    settingsInit();     // load thresholds from NVS first
    sensorInit();
    controlInit();      // outputs safe (pump off) before anything else runs
    inputInit();        // starts encoder task on core 0
    displayInit();

    uint32_t now = millis();
    s_ctrlMs  = now + 4;    // staggered vs. display timers
    s_heartMs = now;

    Serial.printf("Thresholds: low=%.1f high=%.1f inHg\n",
                  settingsLowV10() / 10.0, settingsHighV10() / 10.0);
    Serial.printf("LVGL %d.%d.%d\n",
                  lv_version_major(), lv_version_minor(), lv_version_patch());
}

void loop() {
    displayService();

    // Sensor + pump control every SENSOR_PERIOD_MS — independent of the GUI.
    if (millis() > s_ctrlMs) {
        s_ctrlMs += SENSOR_PERIOD_MS;
        sensorService();
        controlService();
    }

    settingsService();      // flush thresholds to NVS after settle delay

    // Heartbeat LED.
    if (millis() > s_heartMs) {
        s_heartMs += 250;
        digitalWrite(PIN_DEBUG_LED, !digitalRead(PIN_DEBUG_LED));
    }
}
