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

    lcd.print(config.state.value == kStateOn ? "ON-" : "OFF");
    lcd.print(config.frequencyHz.value);
    lcd.print(config.frequencyHz.unit);

    char buffer[4];
    snprintf(buffer, sizeof(buffer), "%03ld", config.carrierFrequencyMicroseconds.value);
    lcd.print(buffer);
    lcd.print("us");

    snprintf(buffer, sizeof(buffer), "%03ld", config.pulsesPerCycle.value);
    lcd.print(buffer);
    lcd.print("P");
    
    lcd.setCursor(0, 1);

    snprintf(buffer, sizeof(buffer), "%03ld", config.interPeakDelayMicroseconds.value);
    lcd.print(buffer);
    lcd.print("us ");
    
    switch (config.symmetry.value)
    {
        case kSymmetryS:
            lcd.print("S");
            break;
        case kSymmetryA:
            lcd.print("A");
            break;
        case kSymmetryR:
        default:
            lcd.print("R");
            break;
    }

    const long groupDelay10Microseconds = config.groupDelay10Microseconds.value;
    char delayBuffer[10];
    snprintf(delayBuffer, sizeof(delayBuffer), " %ldcms", groupDelay10Microseconds);
    lcd.print(delayBuffer);
}

void LcdDisplay::print(const ConfigurationField& field)
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(field.name);
    lcd.setCursor(0, 1);

    if (field.name[0] == 'S' && field.name[1] == 'y')
    {
        lcd.print(static_cast<char>(field.previousValue));
        return;
    }

    lcd.print(field.previousValue);
    lcd.print(" ");
    lcd.print(field.unit);
}

void LcdDisplay::printOutputStatus(bool active)
{
    lcd.setCursor(15, 1);
    lcd.print(active ? "*" : " ");
}