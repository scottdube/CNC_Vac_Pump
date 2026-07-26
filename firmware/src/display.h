// ----------------------------------------------------------------------------
// VacController — display.h
// LVGL 8.3 + TFT_eSPI + SquareLine-generated UI (src/ui_lvgl, from
// jimk3038/CNC_Vac_Pump).  All LVGL calls stay on the main loop's core.
// ----------------------------------------------------------------------------
#pragma once
#include <Arduino.h>

void displayInit();
void displayService();   // call from loop() as often as possible; paces itself
