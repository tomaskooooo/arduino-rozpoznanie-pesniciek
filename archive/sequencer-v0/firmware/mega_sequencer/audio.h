#pragma once
#include <Arduino.h>

void audio_init();
// Starts a capture. The caller must already have sent "LISTEN_START" and the "AUDIO <n>" header.
void audio_start();
bool audio_active();
// Main loop: drains sampled bytes to Serial. Returns false if the capture failed (ring overrun or stall).
bool audio_pump();
// Timer ISR, 8 kHz: starts one ADC conversion while capturing.
void audio_tick();
