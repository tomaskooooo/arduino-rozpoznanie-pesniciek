#include "ui_lcd.h"
#include <LiquidCrystal.h>
#include <string.h>
#include "config.h"
#include "pins.h"

constexpr uint8_t LCD_WIDTH = 16;
constexpr uint8_t SCROLL_GAP = 4;
constexpr uint32_t SCROLL_STEP_MS = 400;

static LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_E, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7);
static char text[2][LCD_MAX_LEN + 1];
static char shown[2][LCD_WIDTH + 1];
static uint8_t scroll_pos[2];
static uint32_t next_scroll_ms[2];

void lcd_init() {
  lcd.begin(LCD_WIDTH, 2);
  lcd.clear();
  memset(shown, 0, sizeof(shown));  // forces the first draw
}

void lcd_set(uint8_t line, const char *s) {
  if (strncmp(text[line], s, LCD_MAX_LEN) == 0) return;
  strncpy(text[line], s, LCD_MAX_LEN);
  text[line][LCD_MAX_LEN] = '\0';
  scroll_pos[line] = 0;
  next_scroll_ms[line] = 0;
}

void lcd_update(uint32_t now) {
  for (uint8_t line = 0; line < 2; line++) {
    uint8_t len = strlen(text[line]);
    uint8_t period = len + SCROLL_GAP;
    if (len > LCD_WIDTH) {
      if (next_scroll_ms[line] == 0) {
        next_scroll_ms[line] = now + 2 * SCROLL_STEP_MS;  // hold the start of the text a moment
      } else if ((int32_t)(now - next_scroll_ms[line]) >= 0) {
        scroll_pos[line] = (scroll_pos[line] + 1) % period;
        next_scroll_ms[line] = now + SCROLL_STEP_MS;
      }
    }
    char window[LCD_WIDTH + 1];
    for (uint8_t i = 0; i < LCD_WIDTH; i++) {
      uint8_t idx = len > LCD_WIDTH ? (scroll_pos[line] + i) % period : i;
      window[i] = idx < len ? text[line][idx] : ' ';
    }
    window[LCD_WIDTH] = '\0';
    if (strcmp(window, shown[line]) != 0) {
      lcd.setCursor(0, line);
      lcd.print(window);
      strcpy(shown[line], window);
    }
  }
}
