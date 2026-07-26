// ----------------------------------------------------------------------------
// VacController — input.cpp
//
// Same decode as the repo's encTask: count on falling edge of A (B high) for
// CW, falling edge of B (A high) for CCW, with a lockout until both lines
// return high.  10 ms poll doubles as debounce.  Runs on core 0 so it never
// contends with LVGL rendering on core 1.
// ----------------------------------------------------------------------------
#include "input.h"
#include "config.h"
#include "settings.h"

static volatile ActiveBug s_active = BUG_GRN;

static void encTask(void *param) {
    bool a, b, sw, swOld = HIGH, lock = false;

    for (;;) {
        a  = digitalRead(PIN_ENC_A);
        b  = digitalRead(PIN_ENC_B);
        sw = digitalRead(PIN_ENC_SW);

        int8_t dir = 0;
        if (!a && b && !lock) { dir = +1; lock = true; }    // CW
        if (a && !b && !lock) { dir = -1; lock = true; }    // CCW
        if (a && b) lock = false;

        if (dir != 0) {
            if (s_active == BUG_RED)
                settingsSetLowV10 (settingsLowV10()  + dir * THRESH_STEP);
            else
                settingsSetHighV10(settingsHighV10() + dir * THRESH_STEP);
        }

        // Push button: toggle which bug is being adjusted.
        if (sw == LOW && swOld == HIGH) {
            s_active = (s_active == BUG_RED) ? BUG_GRN : BUG_RED;
        }
        swOld = sw;

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void inputInit() {
    pinMode(PIN_ENC_A,  INPUT_PULLUP);
    pinMode(PIN_ENC_B,  INPUT_PULLUP);
    pinMode(PIN_ENC_SW, INPUT_PULLUP);

    xTaskCreatePinnedToCore(
        encTask, "Encoder",
        4096,          // stack bytes
        nullptr,
        1,             // priority
        nullptr,
        0);            // core 0 (LVGL + loop() run on core 1)
}

ActiveBug inputActiveBug() { return s_active; }
