#include "sensors.h"
#include "config.h"
#include "pins.h"

static volatile bool knock_flag;
static volatile bool touch_flag;
static volatile uint32_t knock_ms;
static volatile uint32_t touch_ms;

// ISRs only set a flag and a timestamp; the lockout ignores contact bounce.
static void knock_isr() {
  uint32_t now = millis();
  if (now - knock_ms >= KNOCK_LOCKOUT_MS) {
    knock_ms = now;
    knock_flag = true;
  }
}

static void touch_isr() {
  uint32_t now = millis();
  if (now - touch_ms >= TOUCH_LOCKOUT_MS) {
    touch_ms = now;
    touch_flag = true;
  }
}

static bool take(volatile bool &flag) {
  uint8_t sreg = SREG;
  cli();
  bool f = flag;
  flag = false;
  SREG = sreg;
  return f;
}

void sensors_init() {
  pinMode(PIN_KNOCK, INPUT);
  pinMode(PIN_TOUCH, INPUT);
  // CHANGE: the knock module's idle level is not known; the lockout collapses both edges into one hit.
  attachInterrupt(digitalPinToInterrupt(PIN_KNOCK), knock_isr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_TOUCH), touch_isr, RISING);
}

bool sensors_take_knock() {
  return take(knock_flag);
}

bool sensors_take_touch() {
  return take(touch_flag);
}

void sensors_clear() {
  take(knock_flag);
  take(touch_flag);
}
