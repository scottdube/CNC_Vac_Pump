// ----------------------------------------------------------------------------
// VacController — sensor.h
// MPXV6115VC6U read + filter + conversion to inHg x 10.
// ----------------------------------------------------------------------------
#pragma once
#include <Arduino.h>

void    sensorInit();
void    sensorService();        // call every SENSOR_PERIOD_MS
bool    sensorReady();          // true once the filter window has filled
int16_t sensorVacV10();         // filtered vacuum, inHg x 10 (0..VAC_V10_MAX)
uint32_t sensorMilliVolts();    // filtered ADC mV (diagnostics)
