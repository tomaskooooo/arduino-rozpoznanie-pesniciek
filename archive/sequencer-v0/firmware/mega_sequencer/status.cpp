#include "status.h"
#include "config.h"
#include "pins.h"

constexpr uint8_t LEVEL = 90;  // dim enough to look at
constexpr uint32_t BEEP_MS = 80;
constexpr uint32_t BEEP_GAP_MS = 80;

static StatusColor color = STATUS_IDLE;
static int16_t applied = -1;  // last colour written, -1 = none yet
static uint8_t beeps_left;
static bool beep_on;
static uint32_t beep_next_ms;

static void write_rgb(uint8_t r, uint8_t g, uint8_t b) {
  analogWrite(PIN_RGB_R, RGB_COMMON_ANODE ? 255 - r : r);
  analogWrite(PIN_RGB_G, RGB_COMMON_ANODE ? 255 - g : g);
  analogWrite(PIN_RGB_B, RGB_COMMON_ANODE ? 255 - b : b);
}

void status_init() {
  pinMode(PIN_RGB_R, OUTPUT);
  pinMode(PIN_RGB_G, OUTPUT);
  pinMode(PIN_RGB_B, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  write_rgb(0, 0, 0);
}

void status_show(StatusColor c) {
  color = c;
}

void status_beep(uint8_t count) {
  beeps_left = count;
  beep_on = false;
  beep_next_ms = 0;
  digitalWrite(PIN_BUZZER, LOW);
}

void status_update(uint32_t now) {
  int16_t want = color;
  if (color == STATUS_ERROR) want = 100 + ((now / 250) & 1);  // blink phase is part of the key
  if (want != applied) {
    applied = want;
    switch (want) {
      case STATUS_IDLE: write_rgb(0, 0, LEVEL); break;
      case STATUS_PLAYING: write_rgb(0, LEVEL, 0); break;
      case STATUS_RECORDING: write_rgb(LEVEL, 0, 0); break;
      case STATUS_LISTENING: write_rgb(LEVEL * 3 / 4, 0, LEVEL); break;
      case STATUS_BUSY: write_rgb(LEVEL, LEVEL * 2 / 3, 0); break;
      case 100: write_rgb(255, 0, 0); break;
      default: write_rgb(0, 0, 0); break;  // 101: blink-off phase
    }
  }

  if (beep_on) {
    if ((int32_t)(now - beep_next_ms) >= 0) {
      digitalWrite(PIN_BUZZER, LOW);
      beep_on = false;
      beeps_left--;
      beep_next_ms = now + BEEP_GAP_MS;
    }
  } else if (beeps_left > 0 && (int32_t)(now - beep_next_ms) >= 0) {
    digitalWrite(PIN_BUZZER, HIGH);
    beep_on = true;
    beep_next_ms = now + BEEP_MS;
  }
}
