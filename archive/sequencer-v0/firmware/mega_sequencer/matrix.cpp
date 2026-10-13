#include "matrix.h"
#include "pins.h"

static volatile uint8_t framebuffer[8];
static uint8_t scan_col;

void matrix_init() {
  for (uint8_t i = 0; i < 8; i++) {
    pinMode(MATRIX_ROW_PINS[i], OUTPUT);
    digitalWrite(MATRIX_ROW_PINS[i], LOW);
    pinMode(MATRIX_COL_PINS[i], OUTPUT);
    digitalWrite(MATRIX_COL_PINS[i], HIGH);  // column off
  }
}

void matrix_load(const uint8_t cols[8]) {
  uint8_t sreg = SREG;
  cli();
  for (uint8_t c = 0; c < 8; c++) framebuffer[c] = cols[c];
  SREG = sreg;
}

void matrix_scan() {
  digitalWrite(MATRIX_COL_PINS[scan_col], HIGH);  // previous column off
  scan_col = (scan_col + 1) & 7;
  uint8_t bits = framebuffer[scan_col];
  for (uint8_t r = 0; r < 8; r++) digitalWrite(MATRIX_ROW_PINS[r], (bits >> r) & 1);
  digitalWrite(MATRIX_COL_PINS[scan_col], LOW);  // new column on
}
