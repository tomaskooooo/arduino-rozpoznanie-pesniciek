#pragma once
#include <Arduino.h>

struct InputEvents {
  bool play;
  bool mode;
  bool ok;
  bool joy_press;
  int8_t move_x;  // -1, 0, +1
  int8_t move_y;
  bool tempo_changed;
  bool pitch_changed;
};

void inputs_init();
// Fills ev with everything that happened since the last call.
void inputs_update(uint32_t now, InputEvents &ev);
uint16_t inputs_bpm();
uint16_t inputs_pitch();  // 0..1023
