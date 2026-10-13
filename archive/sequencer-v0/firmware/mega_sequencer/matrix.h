#pragma once
#include <Arduino.h>

void matrix_init();
// cols[c] bit r set = LED at column c, row r is on. Safe to call from the main loop.
void matrix_load(const uint8_t cols[8]);
// Lights the next column. Called from the timer ISR at 1 kHz.
void matrix_scan();
