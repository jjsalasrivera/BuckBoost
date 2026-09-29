#pragma once

#include <LiquidCrystal_PCF8574.h>
#include "types.h"

class LcdDisplay {
public:
	explicit LcdDisplay(unsigned char address = 0x27);

	void initialize();
	void print(const char* message, unsigned char column = 0, unsigned char row = 0);
    void clear();
    void print(const TimingConfiguration& config);
	void print(const ConfigurationField& field);

private:
	LiquidCrystal_PCF8574 lcd;
};