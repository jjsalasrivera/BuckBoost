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
    snprintf(buffer, sizeof(buffer), "%03d",
             config.carrierFrequencyMicroseconds.value);
    lcd.print(buffer);
    lcd.print("us");

    snprintf(buffer, sizeof(buffer), "%03d", config.pulsesPerCycle.value);
    lcd.print(buffer);
    lcd.print("P");
    
    lcd.setCursor(0, 1);

    snprintf(buffer, sizeof(buffer), "%03d",
             config.interPeakDelayMicroseconds.value);
    lcd.print(buffer);
    lcd.print("us ");
    
    switch (config.symmetry.value)
    {
        case kSymmetryS:
            lcd.print("SIM");
            break;
        case kSymmetryA:
            lcd.print("ASM");
            break;
        case kSymmetryR:
        default:
            lcd.print("RET");
            break;
    }
    lcd.print(" ");

    snprintf(buffer, sizeof(buffer), "%03d", config.groupDelayMilliseconds.value);
    lcd.print(buffer);
    lcd.print("ms");
}

void LcdDisplay::print(const ConfigurationField& field)
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(field.name);
    lcd.setCursor(0, 1);

    if (field.name[0] == 'S' && field.name[1] == 'y')
    {
        lcd.print(static_cast<char>(field.value));
        return;
    }

    lcd.print(field.value);
    lcd.print(field.unit);
}