#pragma once
#include <Arduino.h>

void lcd_init();
// Sets the text of line 0 or 1. Text longer than 16 characters scrolls.
void lcd_set(uint8_t line, const char *text);
// Redraws whatever changed (scroll step or new text). Call every loop.
void lcd_update(uint32_t now);
