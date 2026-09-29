#pragma once

#include "types.h"
#include "lcd_display.h"

void initializePulseOutputs();

// Desactiva de inmediato los cuatro pines de salida. Se usa como medida de
// seguridad cuando la configuracion es invalida o se rechaza un guardado.
void stopPulseOutputs();

void runPulseOutputs(const TimingConfiguration& config, LcdDisplay& lcd);