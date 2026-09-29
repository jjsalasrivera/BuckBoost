#pragma once

#include "lcd_display.h"
#include "types.h"

constexpr unsigned long kConfigurationTimeoutMilliseconds = 6000;

// Causa por la que una configuracion no se puede usar. Se muestra en el LCD
// para saber que criterio incumplio y no tener que adivinarlo.
enum class ConfigurationError : unsigned char
{
	None,
	FieldRange,
	PulseWidth,
	GroupDelay,
	GroupDuration,
	TimingOverlap
};

struct ConfigurationMenuState
{
	unsigned char fieldIndex;
	unsigned long lastInteractionMilliseconds;
};

void beginConfiguration(TimingConfiguration& config, ConfigurationMenuState& menu, LcdDisplay& lcd, unsigned long nowMilliseconds);

bool handleKey(char key, TimingConfiguration& config, ConfigurationMenuState& menu, LcdDisplay& lcd, unsigned long nowMilliseconds);

bool configurationTimedOut(const ConfigurationMenuState& menu, unsigned long nowMilliseconds);

void loadConfiguration(TimingConfiguration& config, LcdDisplay& lcd);

// Comprueba rangos de todos los campos y tiempos derivados. Recalcula
// config.derived. Devuelve false y escribe el motivo en error.
bool validateConfiguration(const TimingConfiguration& config, ConfigurationError& error);

// Solo tiempos derivados, sin rangos de campo. Recalcula config.derived.
bool hasValidDerivedTiming(const TimingConfiguration& config);

bool saveConfiguration(const TimingConfiguration& config, ConfigurationError& error);