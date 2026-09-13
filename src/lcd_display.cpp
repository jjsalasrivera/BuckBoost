#include "lcd_display.h"
#include "types.h"
#include <Wire.h>

LcdDisplay::LcdDisplay(unsigned char address) : lcd(address) 
{
}

void LcdDisplay::initialize() 
{
    Wire.begin();
    lcd.begin(16, 2);
    lcd.setBacklight(255);
    lcd.noCursor();
    lcd.clear();
}

void LcdDisplay::print(const char* message, unsigned char column, unsigned char row) 
{
    lcd.setCursor(column, row);
    lcd.print(message);
}

void LcdDisplay::clear() 
{
    lcd.clear();
}

void LcdDisplay::print(const TimingConfiguration& config) 
{
    lcd.clear();
    lcd.setCursor(0, 0);

    lcd.print(config.state.value == kStateOn ? "On " : "Off");
    lcd.print(" ");
    
    lcd.print(config.frequencyHz.value);
    lcd.print(config.frequencyHz.unit);
    lcd.print("  ");

    lcd.print(config.carrierFrequencyMicroseconds.value);
    lcd.print(config.carrierFrequencyMicroseconds.unit);
    lcd.print(" ");

    char buffer[4];
    snprintf(buffer, sizeof(buffer), "%03d", config.pulsesPerCycle.value);
    lcd.print(buffer);
    lcd.print(config.pulsesPerCycle.unit);
    
    lcd.setCursor(0, 1);

    snprintf(buffer, sizeof(buffer), "%03d",
             config.interPeakDelayMicroseconds.value);
    lcd.print(buffer);
    lcd.print(config.interPeakDelayMicroseconds.unit);
    lcd.print(" ");
    
    lcd.print(static_cast<char>(config.symmetry.value));
    lcd.print(" ");

    snprintf(buffer, sizeof(buffer), "%03d", config.groupDelayMilliseconds.value);
    lcd.print(buffer);
    lcd.print(config.groupDelayMilliseconds.unit);

    lcd.print("MD");
}