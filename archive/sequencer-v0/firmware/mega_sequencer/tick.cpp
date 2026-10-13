#include "tick.h"
#include <Arduino.h>
#include "audio.h"
#include "matrix.h"

static uint8_t divider;

// Timer5 also backs PWM on D44-D46; those pins are plain digital outputs here.
void tick_init() {
  uint8_t sreg = SREG;
  cli();
  TCCR5A = 0;
  TCCR5B = (1 << WGM52) | (1 << CS51);  // CTC, clk/8 = 2 MHz
  OCR5A = 249;                          // 2 MHz / 250 = 8 kHz
  TCNT5 = 0;
  TIMSK5 = (1 << OCIE5A);
  SREG = sreg;
}

ISR(TIMER5_COMPA_vect) {
  audio_tick();
  if (++divider >= 8) {
    divider = 0;
    matrix_scan();
  }
}
