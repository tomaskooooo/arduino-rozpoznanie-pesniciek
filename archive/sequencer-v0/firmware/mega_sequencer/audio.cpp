#include "audio.h"
#include "config.h"
#include "pins.h"

constexpr uint16_t RING_SIZE = 512;

static volatile uint8_t ring[RING_SIZE];
static volatile uint16_t head;  // written by the ADC ISR
static volatile uint16_t tail;  // written by the main loop
static volatile bool active;
static volatile bool overrun;
static volatile uint32_t started;  // conversions started by audio_tick
static uint32_t sent;
static uint32_t start_ms;

void audio_init() {
  active = false;
}

void audio_start() {
  uint8_t sreg = SREG;
  cli();
  head = 0;
  tail = 0;
  started = 0;
  sent = 0;
  start_ms = millis();
  overrun = false;
  ADMUX = (1 << REFS0) | (1 << ADLAR) | ((PIN_MIC - A0) & 0x07);  // 5V ref, left-adjusted: ADCH = 8 bits
  ADCSRB &= ~(1 << MUX5);
  // ADC clock /64 = 250 kHz: a conversion takes about 58 us. The default /128 needs about 120 us,
  // too close to the 125 us sample period, so ticks would find the ADC still busy and skip samples.
  ADCSRA = (ADCSRA & ~((1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0))) | (1 << ADPS2) | (1 << ADPS1);
  ADCSRA |= (1 << ADIE);
  active = true;
  SREG = sreg;
}

bool audio_active() {
  return active;
}

static void audio_stop() {
  active = false;
  ADCSRA &= ~(1 << ADIE);
  while (ADCSRA & (1 << ADSC)) {}  // let an in-flight conversion finish before analogRead is used again
  ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);  // back to the default /128 for analogRead
}

bool audio_pump() {
  if (!active) return true;
  if (overrun || millis() - start_ms > (AUDIO_SECONDS + 2) * 1000UL) {  // overrun, or the capture stalled
    audio_stop();
    return false;
  }
  while (tail != head && Serial.availableForWrite() > 0) {
    Serial.write(ring[tail]);
    tail = (tail + 1) % RING_SIZE;
    sent++;
  }
  if (sent >= AUDIO_TOTAL_SAMPLES) audio_stop();
  return true;
}

void audio_tick() {
  // Count a start only if the ADC was idle; if a late tick finds it busy the start would be ignored,
  // so skip it and try again next tick (otherwise a sample is lost and the capture never completes).
  if (active && started < AUDIO_TOTAL_SAMPLES && !(ADCSRA & (1 << ADSC))) {
    started++;
    ADCSRA |= (1 << ADSC);
  }
}

ISR(ADC_vect) {
  uint8_t sample = ADCH;
  uint16_t next = (head + 1) % RING_SIZE;
  if (next == tail) {
    overrun = true;
  } else {
    ring[head] = sample;
    head = next;
  }
}
