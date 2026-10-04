#include <Arduino.h>
#include "configuration_menu.h"
#include "lcd_display.h"
#include "keypad_config.h"
#include "precise_delay.h"
#include "pulse_outputs.h"
#include "types.h"

LcdDisplay lcd;
TimingConfiguration config;
ConfigurationMenuState menu;
bool configurationMode = false;

void setup() 
{
    //Serial.begin(115200);
    lcd.initialize();

    // Base de tiempos del Timer1. Debe existir antes de la primera llamada a
    // runPulseOutputs() en loop().
    initializeTimebase();
 
    config.state = {"Estado", kStateOff, kStateOff, kStateOn, kStateOff, ""};
    config.frequencyHz = {"Frequency", 30, 1, 100, 30, "Hz"};
    config.carrierFrequencyMicroseconds = {"Freq, porta", 100, 1, 999, 100, "us"};
    config.pulsesPerCycle = {"Pulses", 10, 1, 999, 10, "P"};
    config.interPeakDelayMicroseconds = {"Delay Pic", 1, 0, 100, 1, "us"};
    config.symmetry = {"Symmetry", kSymmetryR, 0, 0, kSymmetryR, ""};
    config.groupDelay10Microseconds = {"Retardo grupo", 500, 0, 9999, 500, "*10us"};

    // Las salidas se configuran antes de cargar la EEPROM para que los pines
    // queden a nivel bajo incluso si la carga se rechaza.
    initializePulseOutputs();

    loadConfiguration(config, lcd);
    lcd.print(config);
}

void loop() 
{
    const char key = readKeypadKey();
    const unsigned long now = millis();

    runPulseOutputs(config);

    if (!configurationMode)
    {
        // MODO NORMAL: el equipo está generando pulsos y el menú está cerrado.
        // Cualquier tecla sirve para abrir el menú de configuración.
        if (key != kNoKey)
        {
            configurationMode = true;
            beginConfiguration(config, menu, lcd, now);
        }
    }
    else
    {
        // MODO CONFIGURACIÓN: el menú está abierto.
        // Cada tecla pulsada mueve o cambia el campo que se ve en la pantalla.
        if (key != kNoKey)
            handleKey(key, config, menu, lcd, now);

        if (configurationTimedOut(menu, now))
        {
            configurationMode = false;
            lcd.print(config);
        }
    }
}
