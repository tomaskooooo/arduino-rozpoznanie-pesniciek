#include "link.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static char line[LINE_MAX + 1];
static uint8_t line_len;
static bool line_overflow;

void link_init() {
  Serial.begin(SERIAL_BAUD);
}

bool link_printf(const char *fmt, ...) {
  char buf[48];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
  va_end(ap);
  if (n < 0 || n >= (int)sizeof(buf) - 1) return false;
  buf[n++] = '\n';
  if (Serial.availableForWrite() < n) return false;
  Serial.write((const uint8_t *)buf, n);
  return true;
}

static void parse(LinkMsg &msg) {
  msg.type = LINK_BAD;
  if (line_overflow) return;
  line[line_len] = '\0';

  if (strncmp(line, "LCD ", 4) == 0) {
    char *sep = strchr(line + 4, '|');
    if (!sep || strchr(sep + 1, '|')) return;
    *sep = '\0';
    if (strlen(line + 4) > LCD_MAX_LEN || strlen(sep + 1) > LCD_MAX_LEN) return;
    strcpy(msg.line0, line + 4);
    strcpy(msg.line1, sep + 1);
    msg.type = LINK_LCD;
  } else if (strncmp(line, "STATE ", 6) == 0) {
    const char *s = line + 6;
    msg.type = LINK_STATE;
    if (strcmp(s, "IDLE") == 0) msg.state = LINK_STATE_IDLE;
    else if (strcmp(s, "BUSY") == 0) msg.state = LINK_STATE_BUSY;
    else if (strcmp(s, "OK") == 0) msg.state = LINK_STATE_OK;
    else if (strcmp(s, "ERR") == 0) msg.state = LINK_STATE_ERR;
    else msg.type = LINK_BAD;
  }
}

bool link_poll(LinkMsg &msg) {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      parse(msg);
      line_len = 0;
      line_overflow = false;
      return true;
    }
    if (line_len < LINE_MAX) line[line_len++] = c;
    else line_overflow = true;
  }
  return false;
}
