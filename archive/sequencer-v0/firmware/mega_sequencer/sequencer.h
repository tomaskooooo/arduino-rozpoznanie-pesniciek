#pragma once
#include <Arduino.h>

void seq_init();

bool seq_playing();
void seq_play(uint32_t now);
void seq_stop();
// True when a step is due; fills the step index (0-15) and its 8-bit voice mask.
bool seq_tick(uint32_t now, uint16_t bpm, uint8_t &step, uint8_t &mask);

void seq_move_cursor(int8_t dx, int8_t dy);
void seq_toggle_cell();
void seq_toggle_page();

// Cursor position as the LCD shows it: step 1-16, voice 1-8 (8 = top row).
uint8_t seq_cursor_step();
uint8_t seq_cursor_voice();

// Draws the current page, playhead and cursor into the matrix framebuffer.
void seq_render(uint32_t now);
