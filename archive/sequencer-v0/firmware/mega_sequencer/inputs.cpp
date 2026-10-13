#include "inputs.h"
#include <string.h>
#include "config.h"
#include "pins.h"

namespace {

struct Button {
  uint8_t pin;
  bool stable;
  bool last_raw;
  uint32_t changed_ms;
};

struct Pot {
  uint8_t pin;
  int16_t value;
};

struct Axis {
  int8_t dir;
  uint32_t next_ms;
};

Button btn_play, btn_mode, btn_ok, btn_joy;
Pot tempo_pot, pitch_pot;
Axis axis_x, axis_y;
uint32_t next_analog_ms;

void button_init(Button &b, uint8_t pin) {
  b.pin = pin;
  b.stable = true;
  b.last_raw = true;
  b.changed_ms = 0;
  pinMode(pin, INPUT_PULLUP);
}

// True once per press (falling edge after debounce).
bool button_pressed(Button &b, uint32_t now) {
  bool raw = digitalRead(b.pin);
  if (raw != b.last_raw) {
    b.last_raw = raw;
    b.changed_ms = now;
  }
  if (raw != b.stable && now - b.changed_ms >= DEBOUNCE_MS) {
    b.stable = raw;
    return raw == LOW;
  }
  return false;
}

bool pot_moved(Pot &p) {
  int16_t raw = analogRead(p.pin);
  bool at_end = (raw == 0 || raw == 1023) && raw != p.value;
  if (at_end || abs(raw - p.value) >= POT_DEADBAND) {
    p.value = raw;
    return true;
  }
  return false;
}

// Returns -1/0/+1: once on deflection, then auto-repeat while held.
int8_t axis_step(Axis &a, int16_t raw, bool invert, uint32_t now) {
  int8_t dir = raw < JOY_LOW ? -1 : (raw > JOY_HIGH ? 1 : 0);
  if (invert) dir = -dir;
  if (dir == 0) {
    a.dir = 0;
    return 0;
  }
  if (dir != a.dir) {
    a.dir = dir;
    a.next_ms = now + JOY_REPEAT_FIRST_MS;
    return dir;
  }
  if ((int32_t)(now - a.next_ms) >= 0) {
    a.next_ms = now + JOY_REPEAT_MS;
    return dir;
  }
  return 0;
}

}  // namespace

void inputs_init() {
  button_init(btn_play, PIN_BTN_PLAY);
  button_init(btn_mode, PIN_BTN_MODE);
  button_init(btn_ok, PIN_BTN_OK);
  button_init(btn_joy, PIN_JOY_SW);
  tempo_pot.pin = PIN_TEMPO_POT;
  tempo_pot.value = analogRead(PIN_TEMPO_POT);
  pitch_pot.pin = PIN_PITCH_POT;
  pitch_pot.value = analogRead(PIN_PITCH_POT);
  axis_x = {0, 0};
  axis_y = {0, 0};
  next_analog_ms = 0;
}

void inputs_update(uint32_t now, InputEvents &ev) {
  memset(&ev, 0, sizeof(ev));
  ev.play = button_pressed(btn_play, now);
  ev.mode = button_pressed(btn_mode, now);
  ev.ok = button_pressed(btn_ok, now);
  ev.joy_press = button_pressed(btn_joy, now);

  if ((int32_t)(now - next_analog_ms) >= 0) {
    next_analog_ms = now + ANALOG_PERIOD_MS;
    ev.tempo_changed = pot_moved(tempo_pot);
    ev.pitch_changed = pot_moved(pitch_pot);
    ev.move_x = axis_step(axis_x, analogRead(PIN_JOY_X), JOY_INVERT_X, now);
    ev.move_y = axis_step(axis_y, analogRead(PIN_JOY_Y), JOY_INVERT_Y, now);
  }
}

uint16_t inputs_bpm() {
  return map(tempo_pot.value, 0, 1023, BPM_MIN, BPM_MAX);
}

uint16_t inputs_pitch() {
  return pitch_pot.value;
}
