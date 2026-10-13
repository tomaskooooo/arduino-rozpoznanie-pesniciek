#pragma once

// Timer5 at 8 kHz: drives the audio sampler every tick and the matrix scan every 8th tick (1 kHz).
void tick_init();
