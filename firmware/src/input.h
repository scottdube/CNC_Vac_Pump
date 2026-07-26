// ----------------------------------------------------------------------------
// VacController — input.h
// Rotary encoder as a FreeRTOS task (repo model):
//   rotate = adjust active bug (0.5 inHg/detent), push = toggle red/green bug.
// Writes go straight to the settings module.
// ----------------------------------------------------------------------------
#pragma once
#include <Arduino.h>

enum ActiveBug : uint8_t { BUG_RED = 0, BUG_GRN = 1 };

void      inputInit();          // sets up pins and starts the task
ActiveBug inputActiveBug();     // which bug the encoder currently adjusts
