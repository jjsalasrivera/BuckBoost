#include <Arduino.h>
#include <EEPROM.h>

#include "configuration_menu.h"

namespace
{
    constexpr unsigned char kEditableFieldCount = 6;
    constexpr unsigned int kEepromAddress = 0;
    constexpr unsigned int kConfigurationSignature = 0x4255;
    constexpr unsigned char kConfigurationVersion = 1;

    struct StoredConfiguration
    {
        unsigned int signature;
        unsigned char version;
        int frequencyHz;
        int carrierFrequencyMicroseconds;
        int pulsesPerCycle;
        int interPeakDelayMicroseconds;
        int symmetry;
        int groupDelayMilliseconds;
    };

    ConfigurationField* getField(TimingConfiguration& config,
                                 unsigned char fieldIndex)
    {
        switch (fieldIndex)
        {
            case 0: return &config.frequencyHz;
            case 1: return &config.carrierFrequencyMicroseconds;
            case 2: return &config.pulsesPerCycle;
            case 3: return &config.interPeakDelayMicroseconds;
            case 4: return &config.symmetry;
            default: return &config.groupDelayMilliseconds;
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

        return field.value >= field.minValue && field.value <= field.maxValue;
    }

    void changeSymmetry(ConfigurationField& field, int direction)
    {
        if (direction > 0)
        {
            field.value = field.value == kSymmetryR ? kSymmetryS :
                          field.value == kSymmetryS ? kSymmetryA : kSymmetryR;
        }
        else
        {
            field.value = field.value == kSymmetryR ? kSymmetryA :
                          field.value == kSymmetryS ? kSymmetryR : kSymmetryS;
        }
    }

    void changeField(ConfigurationField& field, int direction)
    {
        if (field.name[0] == 'S' && field.name[1] == 'y')
        {
            changeSymmetry(field, direction);
            return;
        }

        field.value += direction;
        if (field.value > field.maxValue)
            field.value = field.minValue;
        else if (field.value < field.minValue)
            field.value = field.maxValue;
    }

    void restoreCurrentField(TimingConfiguration& config,
                             const ConfigurationMenuState& menu)
    {
        getField(config, menu.fieldIndex)->value =
            getField(config, menu.fieldIndex)->previousValue;
    }

    void renderCurrentField(TimingConfiguration& config,
                            const ConfigurationMenuState& menu,
                            LcdDisplay& lcd)
    {
        lcd.print(*getField(config, menu.fieldIndex));
    }
}

void beginConfiguration(TimingConfiguration& config,
                        ConfigurationMenuState& menu,
                        LcdDisplay& lcd,
                        unsigned long nowMilliseconds)
{
    menu.fieldIndex = 0;
    menu.lastInteractionMilliseconds = nowMilliseconds;
    ConfigurationField* field = getField(config, menu.fieldIndex);
    field->previousValue = field->value;
    renderCurrentField(config, menu, lcd);
}

bool handleKey(char key,
               TimingConfiguration& config,
               ConfigurationMenuState& menu,
               LcdDisplay& lcd,
               unsigned long nowMilliseconds)
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
            field->previousValue = field->value;
            menu.fieldIndex = (menu.fieldIndex + 1) % kEditableFieldCount;
            getField(config, menu.fieldIndex)->previousValue =
                getField(config, menu.fieldIndex)->value;
            break;
        case 'N':
            restoreCurrentField(config, menu);
            menu.fieldIndex = (menu.fieldIndex + 1) % kEditableFieldCount;
            getField(config, menu.fieldIndex)->previousValue =
                getField(config, menu.fieldIndex)->value;
            break;
        case 'B':
            restoreCurrentField(config, menu);
            menu.fieldIndex = (menu.fieldIndex + kEditableFieldCount - 1) %
                              kEditableFieldCount;
            getField(config, menu.fieldIndex)->previousValue =
                getField(config, menu.fieldIndex)->value;
            break;
    }

    renderCurrentField(config, menu, lcd);
    return true;
}

bool configurationTimedOut(const ConfigurationMenuState& menu,
                           unsigned long nowMilliseconds)
{
    return nowMilliseconds - menu.lastInteractionMilliseconds >=
           kConfigurationTimeoutMilliseconds;
}

void confirmConfiguration(TimingConfiguration& config,
                          ConfigurationMenuState& menu,
                          LcdDisplay& lcd)
{
    ConfigurationField* field = getField(config, menu.fieldIndex);
    field->previousValue = field->value;
    saveConfiguration(config);
    config.state.value = kStateOn;
    lcd.print(config);
}

void loadConfiguration(TimingConfiguration& config)
{
    StoredConfiguration stored;
    EEPROM.get(kEepromAddress, stored);

    if (stored.signature != kConfigurationSignature ||
        stored.version != kConfigurationVersion)
        return;

    const int values[] = {
        stored.frequencyHz,
        stored.carrierFrequencyMicroseconds,
        stored.pulsesPerCycle,
        stored.interPeakDelayMicroseconds,
        stored.symmetry,
        stored.groupDelayMilliseconds
    };
    ConfigurationField* fields[] = {
        &config.frequencyHz,
        &config.carrierFrequencyMicroseconds,
        &config.pulsesPerCycle,
        &config.interPeakDelayMicroseconds,
        &config.symmetry,
        &config.groupDelayMilliseconds
    };

    for (unsigned char i = 0; i < kEditableFieldCount; ++i)
    {
        ConfigurationField candidate = *fields[i];
        candidate.value = values[i];
        if (!isValidField(candidate))
            return;
    }

    for (unsigned char i = 0; i < kEditableFieldCount; ++i)
    {
        fields[i]->value = values[i];
        fields[i]->previousValue = fields[i]->value;
    }
}

void saveConfiguration(const TimingConfiguration& config)
{
    const StoredConfiguration stored = {
        kConfigurationSignature,
        kConfigurationVersion,
        config.frequencyHz.value,
        config.carrierFrequencyMicroseconds.value,
        config.pulsesPerCycle.value,
        config.interPeakDelayMicroseconds.value,
        config.symmetry.value,
        config.groupDelayMilliseconds.value
    };

    EEPROM.put(kEepromAddress, stored);
}