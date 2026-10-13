#pragma once
#include <Arduino.h>

// Single source of truth for every pin. Each pin is used once.
// D0/D1 are USB serial, D20/D21 (I2C) are unused.

constexpr uint8_t PIN_TEMPO_POT = A0;
constexpr uint8_t PIN_PITCH_POT = A1;
constexpr uint8_t PIN_JOY_X = A2;
constexpr uint8_t PIN_JOY_Y = A3;
constexpr uint8_t PIN_MIC = A4;  // KS0035 sound sensor, the recognition mic

constexpr uint8_t PIN_BTN_PLAY = 2;
constexpr uint8_t PIN_BTN_MODE = 3;
constexpr uint8_t PIN_BTN_OK = 4;
constexpr uint8_t PIN_JOY_SW = 5;

constexpr uint8_t PIN_BUZZER = 6;
constexpr uint8_t PIN_RGB_R = 7;
constexpr uint8_t PIN_RGB_G = 8;
constexpr uint8_t PIN_RGB_B = 9;

constexpr uint8_t PIN_KNOCK = 18;  // INT5
constexpr uint8_t PIN_TOUCH = 19;  // INT4

constexpr uint8_t PIN_LCD_RS = 22;
constexpr uint8_t PIN_LCD_E = 23;
constexpr uint8_t PIN_LCD_D4 = 24;
constexpr uint8_t PIN_LCD_D5 = 25;
constexpr uint8_t PIN_LCD_D6 = 26;
constexpr uint8_t PIN_LCD_D7 = 27;

// Matrix mapping table: index 0..7 = R1..R8 / C1..C8 (rows are anodes through
// 1K, columns are cathodes). If R1/C1 is not the top-left LED, fix it here.
constexpr uint8_t MATRIX_ROW_PINS[8] = {30, 31, 32, 33, 34, 35, 36, 37};
constexpr uint8_t MATRIX_COL_PINS[8] = {38, 39, 40, 41, 42, 43, 44, 45};
