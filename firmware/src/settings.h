// ----------------------------------------------------------------------------
// VacController — settings.h
// Threshold storage: RAM copies + NVS (Preferences) persistence.
// The encoder task writes thresholds; control/display read them; the main
// loop calls settingsService() which flushes to NVS after a settle delay.
// ----------------------------------------------------------------------------
#pragma once
#include <Arduino.h>

void    settingsInit();                 // load from NVS (or defaults)
void    settingsService();              // call periodically from loop()

int16_t settingsLowV10();
int16_t settingsHighV10();

// Set thresholds (clamped, low pushed below high and vice versa, marks dirty).
// Safe to call from the encoder task.
void    settingsSetLowV10(int16_t v);
void    settingsSetHighV10(int16_t v);
