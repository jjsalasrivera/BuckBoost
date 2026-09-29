#include <Arduino.h>
#include <EEPROM.h>

#include "configuration_menu.h"

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
        config.derived.pulsePhaseMicroseconds =
            (config.carrierFrequencyMicroseconds.value -
             (2L * config.interPeakDelayMicroseconds.value)) / 2L;
        config.derived.frequencyPeriodMicroseconds = config.frequencyHz.value > 0
            ? 1000000L / config.frequencyHz.value
            : 0;

        config.derived.groupDelayMicroseconds =
            config.groupDelay10Microseconds.value * 10L;
        config.derived.groupDurationMicroseconds =
            config.pulsesPerCycle.value * config.carrierFrequencyMicroseconds.value;
        config.derived.groupPeriodMicroseconds =
            config.derived.frequencyPeriodMicroseconds -
            config.derived.groupDelayMicroseconds -
            (2L * config.derived.groupDurationMicroseconds);
        config.derived.synchronizedDelayMicroseconds =
            config.derived.frequencyPeriodMicroseconds -
            config.derived.groupDurationMicroseconds;
        config.derived.asymmetricHalfPeriodMicroseconds =
            (config.derived.frequencyPeriodMicroseconds -
             (2L * config.derived.groupDurationMicroseconds)) / 2L;
    }

    bool hasValidDerivedTiming(const TimingConfiguration& config)
    {
        return config.derived.pulsePhaseMicroseconds >= 0 &&
            config.derived.frequencyPeriodMicroseconds > 0 &&
            config.derived.groupDelayMicroseconds >= 0 &&
            config.derived.groupDurationMicroseconds >= 0 &&
            config.derived.groupPeriodMicroseconds >= 0 &&
            config.derived.groupPeriodMicroseconds <= config.derived.frequencyPeriodMicroseconds &&
            config.derived.synchronizedDelayMicroseconds >= 0 &&
            config.derived.asymmetricHalfPeriodMicroseconds >= 0;
    }

    void showConfigurationError(LcdDisplay& lcd)
    {
        lcd.clear();
        lcd.print("CONFIG ERROR");
        lcd.print("Timing overlap", 0, 1);
        delay(5000);
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
            if (!saveConfiguration(config))
            {
                field->value = previousValue;
                field->previousValue = previousValue;
                showConfigurationError(lcd);
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
    {
        ConfigurationField candidateField = *fields[i];
        candidateField.value = values[i];
        if (!isValidField(candidateField))
        {
            calculateDerivedTiming(config);
            showConfigurationError(lcd);
            return;
        }
    }

    for (unsigned char i = 0; i < kEditableFieldCount; ++i)
    {
        fields[i]->value = values[i];
        fields[i]->previousValue = fields[i]->value;
    }

    calculateDerivedTiming(loaded);
    if (!hasValidDerivedTiming(loaded))
    {
        calculateDerivedTiming(config);
        showConfigurationError(lcd);
        return;
    }

    config = loaded;
}

bool saveConfiguration(const TimingConfiguration& config)
{
    TimingConfiguration candidate = config;
    calculateDerivedTiming(candidate);
    if (!hasValidDerivedTiming(candidate))
        return false;

    config.derived = candidate.derived;

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