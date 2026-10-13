#pragma once
#include <Arduino.h>

// One flag switches all debug logging. Debug lines start with "DBG " and are
// NOT part of the PC protocol, so leave this 0 when the Python host is running.
#define DEBUG_SERIAL 0
#if DEBUG_SERIAL
#define DBG(msg) do { Serial.print(F("DBG ")); Serial.println(msg); } while (0)
#else
#define DBG(msg) do {} while (0)
#endif

// Walk R1/C1 through all 64 cells at boot to verify the matrix wiring.
#define BOOT_WALK_TEST 1

constexpr uint32_t SERIAL_BAUD = 115200;

// Sequencer
constexpr uint8_t STEPS = 16;
constexpr uint16_t BPM_MIN = 60;
constexpr uint16_t BPM_MAX = 200;

// Inputs
constexpr uint32_t DEBOUNCE_MS = 25;
constexpr uint32_t ANALOG_PERIOD_MS = 10;
constexpr int16_t POT_DEADBAND = 6;
constexpr int16_t JOY_LOW = 300;
constexpr int16_t JOY_HIGH = 700;
constexpr uint32_t JOY_REPEAT_FIRST_MS = 350;
constexpr uint32_t JOY_REPEAT_MS = 150;
constexpr bool JOY_INVERT_X = false;  // flip if the cursor moves the wrong way
constexpr bool JOY_INVERT_Y = false;

// Knock (digital sensor, so velocity is a fixed value) and touch
constexpr uint32_t KNOCK_LOCKOUT_MS = 60;
constexpr uint32_t TOUCH_LOCKOUT_MS = 400;
constexpr uint8_t HIT_VELOCITY = 200;

// RGB module labelled V R G B: V goes to 5V (common anode), so drive inverted.
constexpr bool RGB_COMMON_ANODE = true;

// Protocol / LCD
constexpr uint8_t LCD_MAX_LEN = 60;  // longest text per LCD line the Mega accepts
constexpr uint8_t LINE_MAX = 128;    // longest serial line the Mega accepts

// Audio capture for song recognition (8 kHz, 8-bit, from PIN_MIC)
constexpr uint16_t AUDIO_SAMPLE_RATE = 8000;
constexpr uint8_t AUDIO_SECONDS = 5;
constexpr uint32_t AUDIO_TOTAL_SAMPLES = (uint32_t)AUDIO_SAMPLE_RATE * AUDIO_SECONDS;
