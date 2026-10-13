#pragma once
#include <Arduino.h>

void sensors_init();
// Each returns true once per event (the ISR only sets a flag).
bool sensors_take_knock();
bool sensors_take_touch();
// Discards pending events.
void sensors_clear();
