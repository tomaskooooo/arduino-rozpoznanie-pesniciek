#pragma once
#include <Arduino.h>
#include "config.h"

// PC -> Mega messages: "LCD <line0>|<line1>" and "STATE <IDLE|BUSY|OK|ERR>".
enum LinkMsgType : uint8_t { LINK_LCD, LINK_STATE, LINK_BAD };
enum LinkState : uint8_t { LINK_STATE_IDLE, LINK_STATE_BUSY, LINK_STATE_OK, LINK_STATE_ERR };

struct LinkMsg {
  LinkMsgType type;
  LinkState state;
  char line0[LCD_MAX_LEN + 1];
  char line1[LCD_MAX_LEN + 1];
};

void link_init();
// Sends one '\n'-terminated line without blocking. Returns false if the TX buffer lacks space.
bool link_printf(const char *fmt, ...);
// Returns true when a complete line arrived; anything malformed comes back as LINK_BAD.
bool link_poll(LinkMsg &msg);
