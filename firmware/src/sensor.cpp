// ----------------------------------------------------------------------------
// VacController — sensor.cpp
//
// Pipeline (per repo approach, with two fixes):
//   analogReadMilliVolts()  ->  median-of-N ring buffer  ->  running average
//   -> undo divider -> datasheet transfer function -> inHg x 10
//
// Fixes vs. repo:
//   * ring-buffer wrap used '>' (off-by-one buffer overflow); now '>='
//   * uses calibrated analogReadMilliVolts() instead of raw counts, so the
//     ESP32's ADC nonlinearity is compensated by the factory eFuse cal
// ----------------------------------------------------------------------------
#include "sensor.h"
#include "config.h"

#ifdef SIM_VACUUM
// ============================================================================
// SIMULATION build — no sensor hardware.  Closed loop against the virtual
// pump: controlPumpOn() true -> vacuum rises, false -> leaks down.
// Serial 'f' toggles a fixture-leak fault that overwhelms the pump so the
// two-stage alarm can be exercised.
// ============================================================================
#include "control.h"

static float s_simV10 = 0.0f;
static bool  s_fault = false;
static uint16_t s_ticks = 0;

void sensorInit() {
    Serial.println("*** SIM_VACUUM build: sensor simulated, ADC bypassed ***");
    Serial.println("*** serial 'f' toggles a fixture-leak fault           ***");
}

void sensorService() {
    if (s_ticks < 50) s_ticks++;                    // ~0.5 s fake warm-up

    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == 'f' || c == 'F') {
            s_fault = !s_fault;
            Serial.printf("SIM fault (fixture leak): %s\n", s_fault ? "ON" : "off");
        }
    }

    if (controlPumpOn())
        s_simV10 += (SIM_PUMP_MAX_V10 - s_simV10) * SIM_PUMP_RATE;
    s_simV10 -= s_simV10 * SIM_LEAK_RATE;           // baseline leak, always
    if (s_fault)
        s_simV10 -= s_simV10 * SIM_FAULT_RATE;      // leak the pump can't beat

    if (s_simV10 < 0) s_simV10 = 0;
    if (s_simV10 > VAC_V10_MAX) s_simV10 = VAC_V10_MAX;
}

bool     sensorReady()      { return s_ticks >= 50; }
uint32_t sensorMilliVolts() { return 0; }

int16_t sensorVacV10() {
    // small dither so needle smoothness / redraw can be judged
    float noise = ((int)(esp_random() % 5) - 2) * 0.1f;
    int32_t v10 = (int32_t)lroundf(s_simV10 + noise);
    if (v10 < 0) v10 = 0;
    if (v10 > VAC_V10_MAX) v10 = VAC_V10_MAX;
    return (int16_t)v10;
}

#else  // ========================= REAL SENSOR =============================

static uint16_t s_raw[SENSOR_MEDIAN_N];     // ring buffer of raw mV samples
static uint16_t s_sorted[SENSOR_MEDIAN_N];  // scratch for median
static uint8_t  s_idx = 0;
static uint16_t s_fill = 0;                 // samples collected so far
static uint32_t s_avgMv = 0;                // running average of median (mV)

static int cmpU16(const void *a, const void *b) {
    return (int)(*(const uint16_t *)a) - (int)(*(const uint16_t *)b);
}

void sensorInit() {
    pinMode(PIN_VAC_SENSOR, INPUT);
    analogSetPinAttenuation(PIN_VAC_SENSOR, ADC_11db);  // full 0..~3.1 V range
}

void sensorService() {
    s_raw[s_idx] = (uint16_t)analogReadMilliVolts(PIN_VAC_SENSOR);
    if (++s_idx >= SENSOR_MEDIAN_N) s_idx = 0;          // (repo bug fixed here)
    if (s_fill < SENSOR_MEDIAN_N * 2) s_fill++;

    memcpy(s_sorted, s_raw, sizeof(s_sorted));
    qsort(s_sorted, SENSOR_MEDIAN_N, sizeof(uint16_t), cmpU16);
    uint16_t median = s_sorted[SENSOR_MEDIAN_N / 2];

    if (s_fill <= SENSOR_MEDIAN_N) {
        s_avgMv = median;                               // seed avg while filling
    } else {
        s_avgMv = (s_avgMv * (SENSOR_AVG_N - 1) + median) / SENSOR_AVG_N;
    }
}

bool sensorReady() {
    return s_fill >= SENSOR_MEDIAN_N * 2;               // window full + avg settled
}

uint32_t sensorMilliVolts() { return s_avgMv; }

int16_t sensorVacV10() {
    // Undo divider: sensor output in mV
    float voutMv = (float)s_avgMv * DIVIDER_GAIN;
    // Transfer function: VOUT = VS*(slope*P + offset)  ->  P (kPa, negative)
    float pKpa = (voutMv / SENSOR_VS_MV - XFER_OFFSET) / XFER_SLOPE;
    // Vacuum in inHg (positive number), x10 integer
    int32_t v10 = (int32_t)lroundf(-pKpa * KPA_TO_INHG * 10.0f);
    if (v10 < 0) v10 = 0;
    if (v10 > VAC_V10_MAX) v10 = VAC_V10_MAX;
    return (int16_t)v10;
}

#endif // SIM_VACUUM
