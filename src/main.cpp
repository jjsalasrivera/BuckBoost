#include <Arduino.h>
#include "configuration_menu.h"
#include "lcd_display.h"
#include "keypad_config.h"
#include "pulse_outputs.h"
#include "types.h"

LcdDisplay lcd;
TimingConfiguration config;
ConfigurationMenuState menu;
bool configurationMode = false;

void setup() 
{
    Serial.begin(115200);
    lcd.initialize();
 
    config.state = {"Estado", kStateOff, kStateOff, kStateOn, kStateOff, ""};
    config.frequencyHz = {"Frequency", 30, 1, 100, 30, "Hz"};
    config.carrierFrequencyMicroseconds = {"Carrier", 100, 1, 999, 100, "us"};
    config.pulsesPerCycle = {"Pulses", 10, 1, 999, 10, "P"};
    config.interPeakDelayMicroseconds = {"Delay Pic", 1, 0, 100, 1, "us"};
    config.symmetry = {"Symmetry", kSymmetryR, 0, 0, kSymmetryR, ""};
    config.groupDelayMilliseconds = {"Retraso personalizado", 500, 0, 10000, 500, "ms"};

    loadConfiguration(config);
    config.state.value = kStateOff;
    lcd.print(config);

    initializePulseOutputs();
}

void loop() 
{
    const char key = readKeypadKey();

    if (!configurationMode)
    {
        if (key != kNoKey)
        {
            config.state.value = kStateOff;
            configurationMode = true;
            runPulseOutputs(config);
            beginConfiguration(config, menu, lcd, millis());
        }
        else
        {
            runPulseOutputs(config);
        }
        return;
    }

    if (key != kNoKey)
        handleKey(key, config, menu, lcd, millis());

    if (configurationTimedOut(menu, millis()))
    {
        confirmConfiguration(config, menu, lcd);
        configurationMode = false;
    }
}
