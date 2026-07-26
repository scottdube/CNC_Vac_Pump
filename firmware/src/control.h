// ----------------------------------------------------------------------------
// VacController — control.h
// Hysteresis pump control.  Independent of the display: keeps working even
// if the GUI stalls.
// ----------------------------------------------------------------------------
#pragma once
#include <Arduino.h>

enum AlarmStage : uint8_t {
    ALARM_NONE  = 0,
    ALARM_WARN  = 1,    // vacuum degrading despite pump — UI warning
    ALARM_PAUSE = 2,    // vacuum critical — alarm relay closed (feed hold)
};

void controlInit();
void controlService();      // call every SENSOR_PERIOD_MS, after sensorService()
bool controlPumpOn();       // current pump state (for the UI)
AlarmStage controlAlarmStage();
bool controlAlarmArmed();   // true once vac has reached the high threshold
