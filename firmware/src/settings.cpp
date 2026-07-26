// ----------------------------------------------------------------------------
// VacController — settings.cpp
// ----------------------------------------------------------------------------
#include "settings.h"
#include "config.h"
#include <Preferences.h>

static Preferences s_prefs;
static volatile int16_t  s_low  = DEFAULT_LOW_V10;
static volatile int16_t  s_high = DEFAULT_HIGH_V10;
static volatile bool     s_dirty = false;
static volatile uint32_t s_lastChangeMs = 0;

static int16_t clampThresh(int16_t v) {
    if (v < THRESH_MIN) return THRESH_MIN;
    if (v > THRESH_MAX) return THRESH_MAX;
    return v;
}

void settingsInit() {
    s_prefs.begin("vacctl", false);
    s_low  = clampThresh(s_prefs.getShort("low",  DEFAULT_LOW_V10));
    s_high = clampThresh(s_prefs.getShort("high", DEFAULT_HIGH_V10));
    if (s_high <= s_low) s_high = clampThresh(s_low + THRESH_STEP);
}

int16_t settingsLowV10()  { return s_low; }
int16_t settingsHighV10() { return s_high; }

void settingsSetLowV10(int16_t v) {
    s_low = clampThresh(v);
    if (s_high <= s_low) s_high = clampThresh(s_low + THRESH_STEP);  // push high
    s_dirty = true;
    s_lastChangeMs = millis();
}

void settingsSetHighV10(int16_t v) {
    s_high = clampThresh(v);
    if (s_low >= s_high) s_low = clampThresh(s_high - THRESH_STEP);  // push low
    s_dirty = true;
    s_lastChangeMs = millis();
}

void settingsService() {
    if (s_dirty && (millis() - s_lastChangeMs) > SETTINGS_SETTLE_MS) {
        s_prefs.putShort("low",  s_low);
        s_prefs.putShort("high", s_high);
        s_dirty = false;
        Serial.printf("Settings saved: low=%d high=%d\n", s_low, s_high);
    }
}
