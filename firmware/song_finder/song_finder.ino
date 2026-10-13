// Song finder: mic + 16x2 LCD + button.
//   button on pin 2 (or 'g') -> stream 5 s of 8 kHz 8-bit audio from the sound sensor on A4 to the PC
//   "L<text>"                -> top LCD row shows <text> (scrolls when longer than 16 characters)
//   "B<0-255>"               -> backlight brightness
//   "S<1-9>"                 -> analyzer sensitivity (1 = bars stay low, 9 = bars jump easily)
//   "D1" / "D0"              -> debug: print the band amplitudes over serial / stop
// The bottom LCD row is a live 16-band spectrum analyzer of the mic (Goertzel filters, no FFT library).
// Wiring: sensor G -> GND, V -> 5V, S -> A4.  LCD RS 22, E 23, D4-D7 24-27, backlight A -> 220 ohm -> pin 9.
// Button: leg -> pin 2, diagonal leg -> GND.
#include <LiquidCrystal.h>
#include <math.h>

const uint8_t PIN_MIC = A4;
const uint8_t PIN_BTN = 2;
const uint8_t PIN_BACKLIGHT = 9;       // PWM pin: LCD pin 15 (A) connects through the 220 ohm resistor to this pin
const uint8_t BACKLIGHT_DEFAULT = 255; // 0 = off ... 255 = full brightness; the PC can change it with "B<0-255>"
const uint32_t BAUD = 250000;
const uint16_t SAMPLE_RATE = 8000;
const uint8_t SECONDS = 5;
const uint32_t TOTAL = (uint32_t)SAMPLE_RATE * SECONDS;
const uint32_t PERIOD_US = 1000000UL / SAMPLE_RATE;

const uint8_t LCD_COLS = 16;
const uint8_t TEXT_MAX = 120;          // longest text for the top row
const uint8_t SCROLL_GAP = 4;          // blank columns between the end of the text and its restart
const uint32_t SCROLL_MS = 350;        // time per scroll step
const uint8_t HOLD_TICKS = 4;          // scroll steps to pause with the start of the text in view
const uint32_t REDRAW_MS = 10000;      // full LCD re-init, so a glitched display recovers by itself

// Spectrum analyzer (bottom row)
const uint8_t BANDS = LCD_COLS;        // one band per LCD column
const uint8_t VIS_N = 128;             // samples per analysis block: 16 ms at 8 kHz
const float BAND_LOW_HZ = 110.0f;      // left column
const float BAND_HIGH_HZ = 3500.0f;    // right column, bands are spaced logarithmically in between
const float GATE_AMP = 12.0f;          // band amplitude (ADC counts) below this shows nothing: keeps noise off the bars
const float MIN_REF = 48.0f;           // the auto-gain never goes more sensitive than this amplitude = full bar
const uint8_t SENS_DEFAULT = 6;        // 1 (bars stay low) ... 9 (bars jump easily); the PC can change it with "S<1-9>"
const float REF_DECAY = 0.97f;         // how fast the auto-gain relaxes after a loud moment, per frame
const uint8_t BAR_PIXELS = 0b01110;    // bar width inside a character cell (0b11111 = full width)

LiquidCrystal lcd(22, 23, 24, 25, 26, 27);  // RS, E, D4, D5, D6, D7

char line[TEXT_MAX + 4];  // "L" + text + NUL
uint8_t len = 0;
uint32_t lastByte = 0;    // when the last serial byte arrived
char title[TEXT_MAX + 1] = "Song finder";
uint8_t pos = 0;          // scroll position of the top row
uint8_t hold = HOLD_TICKS;
uint32_t lastDraw = 0;
uint32_t lastScroll = 0;
bool btnDown = false;
uint32_t btnChange = 0;

int16_t coeff[BANDS];     // Goertzel coefficients, 2*cos(w) in Q10
int16_t block[VIS_N];     // one block of mic samples
float amp[BANDS];         // measured amplitude per band, in ADC counts
uint8_t level[BANDS];     // bar heights currently shown, 0..8
float ref = MIN_REF;      // auto-gain reference: the amplitude that fills a bar
float thr[8];             // thr[k] = fraction of ref a band needs to reach bar height k + 1
bool debug = false;

// Top row: text that fits is static, longer text shows a 16-column window that wraps around.
void drawTitle() {
  uint8_t n = strlen(title);
  lcd.setCursor(0, 0);
  for (uint8_t i = 0; i < LCD_COLS; i++) {
    char c = ' ';
    if (n <= LCD_COLS) {
      if (i < n) c = title[i];
    } else {
      uint8_t k = (pos + i) % (n + SCROLL_GAP);
      if (k < n) c = title[k];
    }
    lcd.write(c);
  }
}

// Bottom row: one custom character per bar height, 1..8 pixels tall. Level 0 is a blank.
void createBarChars() {
  for (uint8_t h = 1; h <= 8; h++) {
    uint8_t rows[8];
    for (uint8_t r = 0; r < 8; r++) rows[r] = (r >= 8 - h) ? BAR_PIXELS : 0;
    lcd.createChar(h - 1, rows);
  }
}

void drawBars() {
  lcd.setCursor(0, 1);
  for (uint8_t b = 0; b < BANDS; b++) {
    lcd.write(level[b] ? (uint8_t)(level[b] - 1) : (uint8_t)' ');
  }
}

// Full init + redraw. The HD44780 can lose its 4-bit alignment after a glitch (power dip, bumped
// wire, noise on E) and only the init sequence recovers it, so this runs every REDRAW_MS, not once.
// The custom bar characters live in the LCD's own memory, so they are rebuilt here as well.
void draw() {
  lcd.begin(16, 2);
  createBarChars();
  drawTitle();
  drawBars();
  lastDraw = millis();
}

void scrollStep() {
  uint8_t n = strlen(title);
  if (n <= LCD_COLS) return;
  if (hold) {
    hold--;
    return;
  }
  pos = (pos + 1) % (n + SCROLL_GAP);
  if (pos == 0) hold = HOLD_TICKS;
  drawTitle();
}

void setText(const char* text) {
  strncpy(title, text, TEXT_MAX);
  title[TEXT_MAX] = 0;
  pos = 0;
  hold = HOLD_TICKS;
  draw();
  lastScroll = millis();
}

void setupBands() {
  float ratio = powf(BAND_HIGH_HZ / BAND_LOW_HZ, 1.0f / (BANDS - 1));
  float f = BAND_LOW_HZ;
  for (uint8_t b = 0; b < BANDS; b++) {
    coeff[b] = (int16_t)lroundf(2.0f * cosf(2.0f * M_PI * f / SAMPLE_RATE) * 1024.0f);
    f *= ratio;
  }
}

// Sensitivity shapes the curve from signal strength to bar height: height = 8 * (amp/ref)^gamma.
// gamma 0.5 (level 9) boosts weak bands so most bars are tall; gamma 2 (level 1) only lets strong peaks reach the top.
void setSensitivity(uint8_t s) {
  s = constrain(s, 1, 9);
  float gamma = 0.5f + (9 - s) * 0.19f;
  for (uint8_t k = 1; k < 8; k++) thr[k] = powf((k + 0.5f) / 8.0f, 1.0f / gamma);
}

// Sample VIS_N points at SAMPLE_RATE and remove the DC offset (the sensor idles near 0 and swings up).
void sampleBlock() {
  uint32_t next = micros();
  int32_t sum = 0;
  for (uint8_t i = 0; i < VIS_N; i++) {
    while ((int32_t)(micros() - next) < 0) {}
    next += PERIOD_US;
    int16_t v = analogRead(PIN_MIC);
    block[i] = v;
    sum += v;
  }
  int16_t mean = sum / VIS_N;
  for (uint8_t i = 0; i < VIS_N; i++) {
    int16_t x = block[i] - mean;
    block[i] = constrain(x, -500, 500);  // the clamp keeps the fixed-point filter below inside 32 bits
  }
}

// One Goertzel filter per band, then map the amplitudes to bar heights.
void analyze() {
  float top = 0;
  for (uint8_t b = 0; b < BANDS; b++) {
    const int32_t c = coeff[b];
    int32_t s1 = 0, s2 = 0;
    for (uint8_t i = 0; i < VIS_N; i++) {
      int32_t s = block[i] + ((c * s1) >> 10) - s2;
      s2 = s1;
      s1 = s;
    }
    float p = (float)s1 * s1 + (float)s2 * s2 - (c / 1024.0f) * (float)s1 * s2;
    amp[b] = sqrtf(p > 0 ? p : 0) * (2.0f / VIS_N);
    if (amp[b] > top) top = amp[b];
  }
  ref = max(top, ref * REF_DECAY);
  if (ref < MIN_REF) ref = MIN_REF;
  for (uint8_t b = 0; b < BANDS; b++) {
    uint8_t lv = 0;
    if (amp[b] >= GATE_AMP) {
      float v = amp[b] / ref;
      lv = 1;
      for (uint8_t k = 1; k < 8; k++) {
        if (v >= thr[k]) lv = k + 1;
      }
    }
    if (level[b] > 0 && lv + 1 < level[b]) lv = level[b] - 1;  // bars fall one step per frame
    level[b] = lv;
  }
}

void printDebug(uint32_t frameUs) {
  Serial.print('V');
  for (uint8_t b = 0; b < BANDS; b++) {
    Serial.print(' ');
    Serial.print((int)amp[b]);
  }
  Serial.print(' ');
  Serial.print((int)ref);
  Serial.print(' ');
  Serial.println(frameUs / 1000);
}

void handleLine() {
  if (len > 1 && line[0] == 'L') {
    setText(line + 1);
  } else if (len > 1 && line[0] == 'B') {
    analogWrite(PIN_BACKLIGHT, constrain(atoi(line + 1), 0, 255));
  } else if (len > 1 && line[0] == 'S') {
    setSensitivity(atoi(line + 1));
  } else if (len > 1 && line[0] == 'D') {
    debug = line[1] == '1';
  }
}

void capture() {
  Serial.print(F("AUDIO "));
  Serial.println(TOTAL);
  uint32_t next = micros();
  for (uint32_t i = 0; i < TOTAL; i++) {
    while ((int32_t)(micros() - next) < 0) {}
    next += PERIOD_US;
    Serial.write((uint8_t)(analogRead(PIN_MIC) >> 2));  // 10-bit -> 8-bit
  }
}

// True once per press (debounced). Internal pull-up: the pin reads LOW while the button is down.
bool buttonPressed() {
  bool down = digitalRead(PIN_BTN) == LOW;
  if (down != btnDown && millis() - btnChange > 30) {
    btnDown = down;
    btnChange = millis();
    return down;
  }
  return false;
}

// Show feedback on the LCD right away, then stream the audio. The PC script answers with the result.
void listen() {
  memset(level, 0, sizeof(level));
  ref = MIN_REF;
  setText("Listening...");
  capture();
  btnDown = digitalRead(PIN_BTN) == LOW;  // a button still held after the capture must not retrigger
  btnChange = millis();
}

void setup() {
  Serial.begin(BAUD);
  pinMode(PIN_BTN, INPUT_PULLUP);
  pinMode(PIN_BACKLIGHT, OUTPUT);
  analogWrite(PIN_BACKLIGHT, BACKLIGHT_DEFAULT);
  ADCSRA = (ADCSRA & ~0x07) | 0x04;  // ADC clock /16: a read takes ~16 us instead of ~112 us
  setupBands();
  setSensitivity(SENS_DEFAULT);
  delay(1000);  // let the LCD settle after power-up/reset before the init sequence
  draw();
  Serial.println(F("BOOT"));
}

void loop() {
  uint32_t frameStart = micros();
  if (buttonPressed()) listen();
  if (len > 0 && millis() - lastByte > 500) len = 0;  // drop a half-received line so it can't swallow the next command
  while (Serial.available()) {
    lastByte = millis();
    char c = Serial.read();
    if (len == 0 && c == 'g') {
      listen();
    } else if (c == '\n') {
      line[len] = 0;
      handleLine();
      len = 0;
    } else if (c != '\r' && len < sizeof(line) - 1) {
      line[len++] = c;
    }
  }
  uint32_t now = millis();
  if (now - lastDraw >= REDRAW_MS) {
    draw();
  } else if (now - lastScroll >= SCROLL_MS) {
    scrollStep();
    lastScroll = now;
  }
  sampleBlock();
  analyze();
  drawBars();
  if (debug) printDebug(micros() - frameStart);
}
