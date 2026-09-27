#pragma once

#include "types.h"
#include "lcd_display.h"

void initializePulseOutputs();
void runPulseOutputs(const TimingConfiguration& config, LcdDisplay& lcd);