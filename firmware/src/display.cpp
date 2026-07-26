// ----------------------------------------------------------------------------
// VacController — display.cpp
//
// Angle conventions are lifted verbatim from the repo (visually validated
// against its gauge artwork):
//   needle:  v10 = 0   -> 2700 (0.1 deg units)      v10 = 300 -> 0
//   bugs:    v10 = 0   -> +1350                     v10 = 300 -> -1350
// The offsets differ because the needle and bug PNGs have different initial
// orientations.  If the artwork is ever redrawn, recalibrate here.
// ----------------------------------------------------------------------------
#include "display.h"
#include "config.h"
#include "sensor.h"
#include "control.h"
#include "settings.h"

#include <TFT_eSPI.h>
#include <lvgl.h>
#include "ui_lvgl/ui.h"

static const uint16_t SCREEN_W = 240;
static const uint16_t SCREEN_H = 240;

static TFT_eSPI s_tft = TFT_eSPI(SCREEN_W, SCREEN_H);
static lv_disp_draw_buf_t s_drawBuf;
static lv_color_t s_buf[SCREEN_W * 10];
static lv_disp_drv_t s_dispDrv;

static uint32_t s_tickMs, s_handlerMs, s_uiMs, s_flashMs;
static bool s_spinnerHidden = false;
static bool s_flash = false;
static lv_color_t s_labelColor;     // label's original color, captured at init

// LCD flush callback for LVGL (repo implementation).
static void dispFlush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *colorP) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    s_tft.startWrite();
    s_tft.setAddrWindow(area->x1, area->y1, w, h);
    s_tft.pushColors((uint16_t *)&colorP->full, w * h, true);
    s_tft.endWrite();
    lv_disp_flush_ready(disp);
}

static int16_t needleAngle(int16_t v10) {
    return (int16_t)map(v10, 0, 300, 2700, 0);
}
static int16_t bugAngle(int16_t v10) {
    return (int16_t)map(v10, 300, 0, -1350, 1350);
}

static void updateBugs() {
    lv_img_set_angle(ui_redBugImg, bugAngle(settingsLowV10()));
    lv_img_set_angle(ui_grnBugImg, bugAngle(settingsHighV10()));
}

void displayInit() {
    lv_init();
    s_tft.begin();
    s_tft.setRotation(0);

    lv_disp_draw_buf_init(&s_drawBuf, s_buf, NULL, SCREEN_W * 10);
    lv_disp_drv_init(&s_dispDrv);
    s_dispDrv.hor_res  = SCREEN_W;
    s_dispDrv.ver_res  = SCREEN_H;
    s_dispDrv.flush_cb = dispFlush;
    s_dispDrv.draw_buf = &s_drawBuf;
    lv_disp_drv_register(&s_dispDrv);

    ui_init();
    updateBugs();                       // place bugs at loaded thresholds
    s_labelColor = lv_obj_get_style_text_color(ui_vacLabel, LV_PART_MAIN);

    uint32_t now = millis();            // staggered so they never coincide
    s_tickMs = now + 1;
    s_handlerMs = now + 2;
    s_uiMs = now + 3;
}

void displayService() {
    // LVGL timekeeping and rendering, paced like the repo main loop.
    if (millis() > s_tickMs)    { lv_tick_inc(LVGL_TICK_MS); s_tickMs += LVGL_TICK_MS; }
    if (millis() > s_handlerMs) { lv_timer_handler();        s_handlerMs += LVGL_TICK_MS; }

    if (millis() > s_uiMs) {
        s_uiMs += UI_UPDATE_MS;

        if (sensorReady()) {
            if (!s_spinnerHidden) {     // boot spinner off once data is valid
                lv_obj_add_flag(ui_Spinner1, LV_OBJ_FLAG_HIDDEN);
                s_spinnerHidden = true;
            }
            int16_t v10 = sensorVacV10();

            char sVal[16];
            snprintf(sVal, sizeof(sVal), "%0.1f", v10 / 10.0);
            lv_label_set_text(ui_vacLabel, sVal);

            lv_img_set_angle(ui_Needle, needleAngle(v10));
        }

        // Pump indicator: red LED image visible while pump runs.
        if (controlPumpOn())
            lv_obj_clear_flag(ui_ledRedImage, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(ui_ledRedImage, LV_OBJ_FLAG_HIDDEN);

        // Alarm: recolor the digital readout by stage.
        //   none -> original color,  warn -> amber,  pause -> red (flashing)
        if (millis() > s_flashMs) { s_flash = !s_flash; s_flashMs = millis() + 300; }
        switch (controlAlarmStage()) {
            case ALARM_WARN:
                lv_obj_set_style_text_color(ui_vacLabel, lv_color_hex(0xFFB000),
                                            LV_PART_MAIN | LV_STATE_DEFAULT);
                break;
            case ALARM_PAUSE:
                lv_obj_set_style_text_color(ui_vacLabel,
                                            s_flash ? lv_color_hex(0xFF2020)
                                                    : lv_color_hex(0x600000),
                                            LV_PART_MAIN | LV_STATE_DEFAULT);
                break;
            default:
                lv_obj_set_style_text_color(ui_vacLabel, s_labelColor,
                                            LV_PART_MAIN | LV_STATE_DEFAULT);
                break;
        }

        updateBugs();                   // cheap; keeps bugs live during adjust
    }
}
