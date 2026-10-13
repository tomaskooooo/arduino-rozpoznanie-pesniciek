#pragma once
#include <Arduino.h>

enum StatusColor : uint8_t {
  STATUS_IDLE,       // blue
  STATUS_PLAYING,    // green
  STATUS_RECORDING,  // red
  STATUS_LISTENING,  // purple
  STATUS_BUSY,       // yellow
  STATUS_ERROR       // red, blinking
};

void status_init();
void status_show(StatusColor color);
// Non-blocking: count short beeps on the active buzzer.
void status_beep(uint8_t count);
void status_update(uint32_t now);
