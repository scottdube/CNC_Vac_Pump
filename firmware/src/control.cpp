// ----------------------------------------------------------------------------
// VacController — control.cpp
//
// Pump rule (hysteresis):
//   vacuum <= low threshold   -> pump ON
//   vacuum >= high threshold  -> pump OFF
//   in between                -> hold previous state
// Pump is forced OFF until the sensor filter has warmed up.
//
// Two-stage alarm (see config.h for levels):
//   Arms only after vacuum first reaches the high threshold, so initial
//   pump-down can't trip it.  Each stage must persist ALARM_QUALIFY_MS
//   before asserting, and clears with ALARM_CLEAR_HYST_V10 of recovery.
//   Stage 2 closes the alarm relay: dry contact to the Tormach digital I/O
//   board input -> feed hold.  The relay stays closed while the condition
//   holds; PathPilot latches the pause on its side, so recovery here does
//   not resume the program — the operator does.
// ----------------------------------------------------------------------------
#include "control.h"
#include "config.h"
#include "sensor.h"
#include "settings.h"

static bool s_pumpOn = false;
static bool s_armed  = false;
static AlarmStage s_stage = ALARM_NONE;
static uint32_t s_warnSinceMs  = 0;   // 0 = condition not currently true
static uint32_t s_pauseSinceMs = 0;

static void driveOutputs() {
    digitalWrite(PIN_PUMP_RELAY,  s_pumpOn ? RELAY_ON : RELAY_OFF);
    digitalWrite(PIN_ALARM_RELAY, (s_stage == ALARM_PAUSE) ? RELAY_ON : RELAY_OFF);
}

void controlInit() {
    pinMode(PIN_PUMP_RELAY, OUTPUT);
    pinMode(PIN_ALARM_RELAY, OUTPUT);
    digitalWrite(PIN_PUMP_RELAY, RELAY_OFF);    // boot-safe: everything off
    digitalWrite(PIN_ALARM_RELAY, RELAY_OFF);
}

// Qualify a condition: returns true only after it has been continuously
// true for ALARM_QUALIFY_MS.  'sinceMs' holds the first-true timestamp.
static bool qualified(bool cond, uint32_t &sinceMs) {
    if (!cond) { sinceMs = 0; return false; }
    if (sinceMs == 0) sinceMs = millis();
    return (millis() - sinceMs) >= ALARM_QUALIFY_MS;
}

void controlService() {
    if (!sensorReady()) {                       // no valid reading yet
        s_pumpOn = false;
        s_stage  = ALARM_NONE;
        driveOutputs();
        return;
    }

    int16_t v10  = sensorVacV10();
    int16_t low  = settingsLowV10();
    int16_t high = settingsHighV10();

    // --- Pump hysteresis ---
    if (v10 <= low)  s_pumpOn = true;
    if (v10 >= high) s_pumpOn = false;

    // --- Alarm arming ---
    if (!s_armed && v10 >= high) {
        s_armed = true;
        Serial.println("Alarm armed (vacuum reached high threshold)");
    }

    // --- Two-stage alarm ---
    if (s_armed) {
        int16_t warnLevel  = low - ALARM_WARN_OFFSET_V10;
        int16_t pauseLevel = low - ALARM_PAUSE_OFFSET_V10;

        bool warnQ  = qualified(v10 < warnLevel,  s_warnSinceMs);
        bool pauseQ = qualified(v10 < pauseLevel, s_pauseSinceMs);

        switch (s_stage) {
            case ALARM_NONE:
                if (pauseQ)      s_stage = ALARM_PAUSE;
                else if (warnQ)  s_stage = ALARM_WARN;
                break;
            case ALARM_WARN:
                if (pauseQ)                                          s_stage = ALARM_PAUSE;
                else if (v10 >= warnLevel + ALARM_CLEAR_HYST_V10)    s_stage = ALARM_NONE;
                break;
            case ALARM_PAUSE:
                if (v10 >= pauseLevel + ALARM_CLEAR_HYST_V10)
                    s_stage = warnQ ? ALARM_WARN : ALARM_NONE;
                break;
        }
    }

    driveOutputs();
}

bool controlPumpOn()            { return s_pumpOn; }
AlarmStage controlAlarmStage()  { return s_stage; }
bool controlAlarmArmed()        { return s_armed; }
