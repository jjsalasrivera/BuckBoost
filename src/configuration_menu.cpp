#include <Arduino.h>
#include <EEPROM.h>

#include "configuration_menu.h"
#include "pulse_outputs.h"

namespace
{
    constexpr unsigned char kEditableFieldCount = 7;
    constexpr unsigned int kEepromAddress = 0;
    constexpr unsigned int kConfigurationSignature = 0x4254;
    constexpr unsigned char kConfigurationVersion = 3;

    struct StoredConfiguration
    {
        unsigned int signature;
        unsigned char version;
        long state;
        long frequencyHz;
        long carrierFrequencyMicroseconds;
        long pulsesPerCycle;
        long interPeakDelayMicroseconds;
        long symmetry;
        long groupDelay10Microseconds;
    };

    ConfigurationField* getField(TimingConfiguration& config, unsigned char fieldIndex)
    {
        switch (fieldIndex)
        {
            case 0: return &config.state;
            case 1: return &config.frequencyHz;
            case 2: return &config.carrierFrequencyMicroseconds;
            case 3: return &config.pulsesPerCycle;
            case 4: return &config.interPeakDelayMicroseconds;
            case 5: return &config.symmetry;
            default: return &config.groupDelay10Microseconds;
        }
    }

    bool isValidSymmetry(int value)
    {
        return value == kSymmetryR || value == kSymmetryS || value == kSymmetryA;
    }

    bool isValidField(const ConfigurationField& field)
    {
        if (field.name[0] == 'S' && field.name[1] == 'y')
            return isValidSymmetry(field.value);

        if (field.name[0] == 'E' && field.name[1] == 's')
            return field.value == kStateOff || field.value == kStateOn;

        return field.value >= field.minValue && field.value <= field.maxValue;
    }

    void changeSymmetry(ConfigurationField& field, int direction)
    {
        if (direction > 0)
        {
            field.previousValue = field.previousValue == kSymmetryR ? kSymmetryS :
                          field.previousValue == kSymmetryS ? kSymmetryA : kSymmetryR;
        }
        else
        {
            field.previousValue = field.previousValue == kSymmetryR ? kSymmetryA :
                          field.previousValue == kSymmetryS ? kSymmetryR : kSymmetryS;
        }
    }

    void changeField(ConfigurationField& field, int direction)
    {
        if (field.name[0] == 'S' && field.name[1] == 'y')
        {
            changeSymmetry(field, direction);
            return;
        }

        field.previousValue += direction;
        if (field.previousValue > field.maxValue)
            field.previousValue = field.minValue;
        else if (field.previousValue < field.minValue)
            field.previousValue = field.maxValue;
    }

    void renderCurrentField(TimingConfiguration& config, const ConfigurationMenuState& menu, LcdDisplay& lcd)
    {
        lcd.print(*getField(config, menu.fieldIndex));
    }

    void calculateDerivedTiming(const TimingConfiguration& config)
    {
        config.derived.pulsePhaseMicroseconds = (config.carrierFrequencyMicroseconds.value - (2L * config.interPeakDelayMicroseconds.value)) / 2L;
        config.derived.frequencyPeriodMicroseconds = config.frequencyHz.value > 0 ? 1000000L / config.frequencyHz.value : 0;
        config.derived.groupDelayMicroseconds = config.groupDelay10Microseconds.value * 10L;
        config.derived.groupDurationMicroseconds = config.pulsesPerCycle.value * config.carrierFrequencyMicroseconds.value;
        config.derived.groupPeriodMicroseconds = config.derived.frequencyPeriodMicroseconds - config.derived.groupDelayMicroseconds - (2L * config.derived.groupDurationMicroseconds);
        config.derived.asymmetricHalfPeriodMicroseconds = (config.derived.frequencyPeriodMicroseconds - (2L * config.derived.groupDurationMicroseconds)) / 2L;
    }

    bool validateFields(const TimingConfiguration& config, ConfigurationError& error)
    {
        const ConfigurationField* fields[] = {
            &config.state,
            &config.frequencyHz,
            &config.carrierFrequencyMicroseconds,
            &config.pulsesPerCycle,
            &config.interPeakDelayMicroseconds,
            &config.symmetry,
            &config.groupDelay10Microseconds
        };

        for (unsigned char i = 0; i < kEditableFieldCount; ++i)
        {
            if (!isValidField(*fields[i]))
            {
                error = ConfigurationError::FieldRange;
                return false;
            }
        }

        return true;
    }

    // config.derived es mutable, asi que se recalcula sobre la propia
    // configuracion sin necesidad de una copia.
    bool validateDerivedTiming(const TimingConfiguration& config, ConfigurationError& error)
    {
        calculateDerivedTiming(config);
        const DerivedTiming& derived = config.derived;

        // El periodo de la frecuencia tiene que existir.
        if (derived.frequencyPeriodMicroseconds <= 0)
        {
            error = ConfigurationError::FieldRange;
            return false;
        }

        // Cada pulso necesita fase positiva: portadora > 2 * retardo entre picos.
        if (derived.pulsePhaseMicroseconds <= 0)
        {
            error = ConfigurationError::PulseWidth;
            return false;
        }

        // Un grupo debe tener al menos un pulso.
        if (derived.groupDurationMicroseconds <= 0)
        {
            error = ConfigurationError::GroupDuration;
            return false;
        }

        // Criterio explicito: el retardo entre grupos no puede superar el
        // periodo de la frecuencia.
        if (derived.groupDelayMicroseconds > derived.frequencyPeriodMicroseconds)
        {
            error = ConfigurationError::GroupDelay;
            return false;
        }

        // Un grupo completo tiene que caber en el periodo.
        if (derived.groupDurationMicroseconds > derived.frequencyPeriodMicroseconds)
        {
            error = ConfigurationError::GroupDuration;
            return false;
        }

        // Simetria R: retardo + 2 grupos dentro del periodo.
        if (derived.groupPeriodMicroseconds < 0)
        {
            error = ConfigurationError::TimingOverlap;
            return false;
        }

        // Simetria A: 2 grupos por periodo.
        // Este no se puede deducir de la comprobacion de groupDuration de
        // arriba, que solo mira un grupo contra el periodo y aqui caben dos.
        if (derived.asymmetricHalfPeriodMicroseconds < 0)
        {
            error = ConfigurationError::TimingOverlap;
            return false;
        }

        return true;
    }

    const char* errorDescription(ConfigurationError error)
    {
        switch (error)
        {
            case ConfigurationError::FieldRange: return "Bad value range";
            case ConfigurationError::PulseWidth: return "Width < 2x delay";
            case ConfigurationError::GroupDelay: return "Group delay > T";
            case ConfigurationError::GroupDuration: return "Pulses do not fit";
            case ConfigurationError::TimingOverlap: return "Timing overlap";
            case ConfigurationError::None:
            default: return "Unknown error";
        }
    }

    void showConfigurationError(LcdDisplay& lcd, ConfigurationError error)
    {
        lcd.clear();
        lcd.print("CONFIG ERROR");
        lcd.print(errorDescription(error), 0, 1);
        delay(5000);
    }

    // Rechazo de una configuracion: se para la salida de pulsos, el estado
    // queda apagado en RAM (sin tocar la EEPROM) y se informa durante 5 s.
    void rejectConfiguration(TimingConfiguration& config, LcdDisplay& lcd, ConfigurationError error)
    {
        config.state.value = kStateOff;
        config.state.previousValue = kStateOff;

        calculateDerivedTiming(config);
        stopPulseOutputs();
        showConfigurationError(lcd, error);
    }
}

void beginConfiguration(TimingConfiguration& config, ConfigurationMenuState& menu, LcdDisplay& lcd, unsigned long nowMilliseconds)
{
    menu.fieldIndex = 0;
    menu.lastInteractionMilliseconds = nowMilliseconds;
    ConfigurationField* field = getField(config, menu.fieldIndex);
    field->previousValue = field->value;
    renderCurrentField(config, menu, lcd);
}

bool handleKey(char key, TimingConfiguration& config, ConfigurationMenuState& menu, LcdDisplay& lcd, unsigned long nowMilliseconds)
{    
    if (key != 'N' && key != 'B' && key != 'I' && key != 'D' && key != 'S')
        return false;

    ConfigurationField* field = getField(config, menu.fieldIndex);
    menu.lastInteractionMilliseconds = nowMilliseconds;

    switch (key)
    {
        case 'I':
            changeField(*field, 1);
            
            break;
        case 'D':
            changeField(*field, -1);
            
            break;
        case 'S':
        {
            const long previousValue = field->value;
            field->value = field->previousValue;

            ConfigurationError error = ConfigurationError::None;
            if (!saveConfiguration(config, error))
            {
                field->value = previousValue;
                field->previousValue = previousValue;
                rejectConfiguration(config, lcd, error);
            }

            break;
        }
        case 'N':
            //restoreCurrentField(config, menu);
            field->previousValue = field->value;

            menu.fieldIndex = (menu.fieldIndex + 1) % kEditableFieldCount;
            getField(config, menu.fieldIndex)->previousValue = getField(config, menu.fieldIndex)->value;
            
            break;
        case 'B':
            //restoreCurrentField(config, menu);
            field->previousValue = field->value;

            menu.fieldIndex = (menu.fieldIndex + kEditableFieldCount - 1) % kEditableFieldCount;
            getField(config, menu.fieldIndex)->previousValue = getField(config, menu.fieldIndex)->value;
            
            break;
    }

    renderCurrentField(config, menu, lcd);
    return true;
}

bool configurationTimedOut(const ConfigurationMenuState& menu, unsigned long nowMilliseconds)
{
    return nowMilliseconds - menu.lastInteractionMilliseconds >= kConfigurationTimeoutMilliseconds;
}

void loadConfiguration(TimingConfiguration& config, LcdDisplay& lcd)
{
    StoredConfiguration stored;
    EEPROM.get(kEepromAddress, stored);

    if (stored.signature != kConfigurationSignature ||
        stored.version != kConfigurationVersion)
    {
        calculateDerivedTiming(config);
        return;
    }

    const long values[] = {
        stored.state,
        stored.frequencyHz,
        stored.carrierFrequencyMicroseconds,
        stored.pulsesPerCycle,
        stored.interPeakDelayMicroseconds,
        stored.symmetry,
        stored.groupDelay10Microseconds
    };
    TimingConfiguration loaded = config;
    ConfigurationField* fields[] = {
        &loaded.state,
        &loaded.frequencyHz,
        &loaded.carrierFrequencyMicroseconds,
        &loaded.pulsesPerCycle,
        &loaded.interPeakDelayMicroseconds,
        &loaded.symmetry,
        &loaded.groupDelay10Microseconds
    };

    for (unsigned char i = 0; i < kEditableFieldCount; ++i)
        fields[i]->value = values[i];

    ConfigurationError error = ConfigurationError::None;
    if (!validateConfiguration(loaded, error))
    {
        calculateDerivedTiming(config);
        rejectConfiguration(config, lcd, error);
        return;
    }

    for (unsigned char i = 0; i < kEditableFieldCount; ++i)
        fields[i]->previousValue = fields[i]->value;

    config = loaded;
}

bool validateConfiguration(const TimingConfiguration& config, ConfigurationError& error)
{
    if (!validateFields(config, error))
        return false;

    return validateDerivedTiming(config, error);
}

bool hasValidDerivedTiming(const TimingConfiguration& config)
{
    ConfigurationError error = ConfigurationError::None;
    return validateDerivedTiming(config, error);
}

bool saveConfiguration(const TimingConfiguration& config, ConfigurationError& error)
{
    if (!validateConfiguration(config, error))
        return false;

    const StoredConfiguration stored = {
        kConfigurationSignature,
        kConfigurationVersion,
        config.state.value,
        config.frequencyHz.value,
        config.carrierFrequencyMicroseconds.value,
        config.pulsesPerCycle.value,
        config.interPeakDelayMicroseconds.value,
        config.symmetry.value,
        config.groupDelay10Microseconds.value
    };

    EEPROM.put(kEepromAddress, stored);
    return true;
}