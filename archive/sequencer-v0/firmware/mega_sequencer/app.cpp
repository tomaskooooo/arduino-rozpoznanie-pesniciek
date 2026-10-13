#include "app.h"
#include <stdio.h>
#include "audio.h"
#include "config.h"
#include "inputs.h"
#include "link.h"
#include "matrix.h"
#include "sensors.h"
#include "sequencer.h"
#include "status.h"
#include "tick.h"
#include "ui_lcd.h"

enum Mode : uint8_t { MODE_SEQ, MODE_REC, MODE_LISTEN };
static const char *const MODE_NAMES[] = {"SEQ", "REC", "LISTEN"};

constexpr uint32_t RENDER_PERIOD_MS = 30;
constexpr uint32_t LCD_STATUS_PERIOD_MS = 100;

static Mode mode = MODE_SEQ;
static bool text_override;  // PC-supplied or error text is on the LCD
static bool error_active;
static bool busy;           // PC is working (STATE BUSY)
static bool capturing;
static uint32_t next_render_ms;
static uint32_t next_lcd_ms;

static void app_error(const char *line0, const char *line1) {
  if (!error_active) status_beep(2);
  error_active = true;
  text_override = true;
  lcd_set(0, line0);
  lcd_set(1, line1);
}

#define TX(...) do { if (!link_printf(__VA_ARGS__)) app_error("TX FULL", ""); } while (0)

static void set_mode(Mode m) {
  mode = m;
  text_override = false;
  error_active = false;
  busy = false;
  if (m == MODE_LISTEN && seq_playing()) {
    seq_stop();
    TX("PLAY 0");
  }
  TX("MODE %s", MODE_NAMES[m]);
  status_beep(1);
}

static void boot_walk() {
#if BOOT_WALK_TEST
  // Walks R1/C1 through all 64 cells: column 1 top to bottom, then column 2, ...
  lcd_set(0, "MATRIX TEST");
  lcd_set(1, "R1/C1 walking");
  lcd_update(millis());
  for (uint8_t i = 0; i < 64; i++) {
    uint8_t cols[8] = {0};
    cols[i / 8] = 1 << (i % 8);
    matrix_load(cols);
    uint32_t t = millis();
    while (millis() - t < 60) {}
  }
  uint8_t blank[8] = {0};
  matrix_load(blank);
#endif
}

static void start_listening() {
  if (!link_printf("LISTEN_START") || !link_printf("AUDIO %lu", (unsigned long)AUDIO_TOTAL_SAMPLES)) {
    app_error("TX FULL", "");
    return;
  }
  sensors_clear();
  text_override = true;
  lcd_set(0, "LISTENING...");
  lcd_set(1, "");
  capturing = true;
  audio_start();
}

static void handle_link(const LinkMsg &m) {
  switch (m.type) {
    case LINK_LCD:
      text_override = true;
      lcd_set(0, m.line0);
      lcd_set(1, m.line1);
      break;
    case LINK_STATE:
      switch (m.state) {
        case LINK_STATE_IDLE:
          busy = false;
          error_active = false;
          text_override = false;
          break;
        case LINK_STATE_BUSY:
          busy = true;
          break;
        case LINK_STATE_OK:
          busy = false;
          error_active = false;
          status_beep(1);
          break;
        case LINK_STATE_ERR:
          busy = false;
          app_error("PC ERROR", "");
          break;
      }
      break;
    case LINK_BAD:
      app_error("PROTO ERR", "");
      break;
  }
}

static void handle_inputs(uint32_t now) {
  InputEvents ev;
  inputs_update(now, ev);

  if (ev.mode) set_mode((Mode)((mode + 1) % 3));

  if (ev.play && mode != MODE_LISTEN) {
    if (seq_playing()) seq_stop();
    else seq_play(now);
    TX("PLAY %d", seq_playing() ? 1 : 0);
  }

  if (mode == MODE_SEQ && ev.ok) seq_toggle_page();
  if (mode != MODE_LISTEN) {
    if (ev.move_x || ev.move_y) seq_move_cursor(ev.move_x, ev.move_y);
    if (ev.joy_press) seq_toggle_cell();
  }

  if (ev.tempo_changed) TX("TEMPO %u", inputs_bpm());
  if (ev.pitch_changed) TX("PITCH %u", inputs_pitch());

  if (sensors_take_knock() && mode != MODE_LISTEN) TX("HIT %u", HIT_VELOCITY);
  if (sensors_take_touch()) {
    if (mode == MODE_REC) TX("REPLAY");
    else if (mode == MODE_LISTEN) start_listening();
  }
}

static void update_display(uint32_t now) {
  if ((int32_t)(now - next_render_ms) >= 0) {
    next_render_ms = now + RENDER_PERIOD_MS;
    seq_render(now);
  }

  if (!text_override && (int32_t)(now - next_lcd_ms) >= 0) {
    next_lcd_ms = now + LCD_STATUS_PERIOD_MS;
    char l0[LCD_MAX_LEN + 1];
    char l1[LCD_MAX_LEN + 1];
    snprintf(l0, sizeof(l0), "%s %ubpm %c", MODE_NAMES[mode], inputs_bpm(), seq_playing() ? '>' : '.');
    snprintf(l1, sizeof(l1), "Step%2u Voice%u", seq_cursor_step(), seq_cursor_voice());
    lcd_set(0, l0);
    lcd_set(1, l1);
  }
  lcd_update(now);

  if (error_active) status_show(STATUS_ERROR);
  else if (busy) status_show(STATUS_BUSY);
  else if (mode == MODE_REC) status_show(STATUS_RECORDING);
  else if (mode == MODE_LISTEN) status_show(STATUS_LISTENING);
  else status_show(seq_playing() ? STATUS_PLAYING : STATUS_IDLE);
}

void app_init() {
  link_init();
  status_init();
  matrix_init();
  lcd_init();
  inputs_init();
  sensors_init();
  seq_init();
  audio_init();
  tick_init();  // starts the matrix scan
  boot_walk();

  // Tell the PC the starting state (the PC's serial open resets the Mega, so it sees this).
  TX("MODE %s", MODE_NAMES[mode]);
  TX("PLAY 0");
  TX("TEMPO %u", inputs_bpm());
  TX("PITCH %u", inputs_pitch());
}

void app_update() {
  uint32_t now = millis();

  if (capturing) {
    // While audio bytes are streaming, nothing else may write to Serial.
    if (!audio_pump()) {
      capturing = false;
      app_error("AUDIO ERROR", "");
    } else if (!audio_active()) {
      capturing = false;
      sensors_clear();
      busy = true;
      lcd_set(0, "IDENTIFYING...");
      lcd_set(1, "");
    }
    status_update(now);
    return;
  }

  LinkMsg msg;
  while (link_poll(msg)) handle_link(msg);

  handle_inputs(now);

  uint8_t step, mask;
  if (seq_tick(now, inputs_bpm(), step, mask)) TX("STEP %u %u", step, mask);

  update_display(now);
  status_update(now);
}
