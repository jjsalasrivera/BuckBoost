#pragma once

#include "lcd_display.h"
#include "types.h"

constexpr unsigned long kConfigurationTimeoutMilliseconds = 2000;

struct ConfigurationMenuState
{
	unsigned char fieldIndex;
	unsigned long lastInteractionMilliseconds;
};

void beginConfiguration(TimingConfiguration& config,
	ConfigurationMenuState& menu,
	LcdDisplay& lcd,
	unsigned long nowMilliseconds);
bool handleKey(char key,
	TimingConfiguration& config,
	ConfigurationMenuState& menu,
	LcdDisplay& lcd,
	unsigned long nowMilliseconds);
bool configurationTimedOut(const ConfigurationMenuState& menu,
	unsigned long nowMilliseconds);
void confirmConfiguration(TimingConfiguration& config,
	ConfigurationMenuState& menu,
	LcdDisplay& lcd);
void loadConfiguration(TimingConfiguration& config);
void saveConfiguration(const TimingConfiguration& config);