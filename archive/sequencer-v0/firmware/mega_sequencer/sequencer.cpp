#include "sequencer.h"
#include <string.h>
#include "config.h"
#include "matrix.h"

// pattern[step] bit v = voice v is on. Voice 7 is drawn on matrix row 0 (top).
static uint8_t pattern[STEPS];
static uint8_t page;  // 0 = steps 1-8, 1 = steps 9-16
static uint8_t cursor_x;  // step within the page, 0-7
static uint8_t cursor_y;  // matrix row, 0-7
static bool playing;
static uint8_t current_step;
static uint32_t next_step_ms;

static uint8_t reverse_bits(uint8_t v) {
  uint8_t r = 0;
  for (uint8_t i = 0; i < 8; i++) {
    r = (r << 1) | (v & 1);
    v >>= 1;
  }
  return r;
}

void seq_init() {
  memset(pattern, 0, sizeof(pattern));
  page = 0;
  cursor_x = 0;
  cursor_y = 0;
  playing = false;
}

bool seq_playing() {
  return playing;
}

void seq_play(uint32_t now) {
  playing = true;
  current_step = STEPS - 1;  // first tick advances to step 0
  next_step_ms = now;
}

void seq_stop() {
  playing = false;
}

bool seq_tick(uint32_t now, uint16_t bpm, uint8_t &step, uint8_t &mask) {
  if (!playing || (int32_t)(now - next_step_ms) < 0) return false;
  next_step_ms += 15000UL / bpm;  // 16th notes: 4 steps per beat; accumulating avoids drift
  current_step = (current_step + 1) % STEPS;
  step = current_step;
  mask = pattern[current_step];
  return true;
}

void seq_move_cursor(int8_t dx, int8_t dy) {
  cursor_x = constrain((int8_t)cursor_x + dx, 0, 7);
  cursor_y = constrain((int8_t)cursor_y + dy, 0, 7);
}

void seq_toggle_cell() {
  pattern[page * 8 + cursor_x] ^= 1 << (7 - cursor_y);
}

void seq_toggle_page() {
  page ^= 1;
}

uint8_t seq_cursor_step() {
  return page * 8 + cursor_x + 1;
}

uint8_t seq_cursor_voice() {
  return 8 - cursor_y;
}

void seq_render(uint32_t now) {
  uint8_t cols[8];
  for (uint8_t c = 0; c < 8; c++) cols[c] = reverse_bits(pattern[page * 8 + c]);
  if (playing && current_step / 8 == page) cols[current_step % 8] ^= 0xFF;  // playhead inverts its column
  if ((now / 250) & 1) cols[cursor_x] ^= 1 << cursor_y;                       // blinking cursor
  matrix_load(cols);
}
