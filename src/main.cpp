#include <Arduino.h>
#include "configuration_menu.h"
#include "lcd_display.h"
#include "keypad_config.h"
#include "pulse_outputs.h"
#include "types.h"

LcdDisplay lcd;
TimingConfiguration config;

void setup() 
{
    Serial.begin(115200);
    lcd.initialize();
 
    config.state = {"Estado", kStateOff, kStateOff, kStateOn, kStateOff, ""};
    config.frequencyHz = {"Frequency", 30, 1, 100, 30, "Hz"};
    config.carrierFrequencyMicroseconds = {"Carrier", 100, 1, 999, 100, "us"};
    config.pulsesPerCycle = {"Pulses", 10, 1, 999, 10, "Pu"};
    config.interPeakDelayMicroseconds = {"Delay Pic", 1, 0, 100, 1, "us"};
    config.symmetry = {"Symmetry", kSymmetryR, 0, 0, kSymmetryR, ""};
    config.groupDelayMilliseconds = {"Retraso personalizado", 500, 0, 10000, 500, "us"};
    
    lcd.print(config);

    initializePulseOutputs();
}

void loop() 
{
    const char key = readKeypadKey();
    if (key != kNoKey)
        handleKey(key, config, lcd);

    runPulseOutputs(config);
}
