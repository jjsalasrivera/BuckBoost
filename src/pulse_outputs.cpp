#include <Arduino.h>

#include "pulse_outputs.h"

namespace 
{
    // Pines 6, 7, 8 y 9 (PH3, PH4, PH5 y PH6) para el grupo 1.
    constexpr PulseOutputGroup group1{
        {&DDRH, &PORTH, B00101000},
        {&DDRH, &PORTH, B01010000}
    };

    // Pines 10, 11, 12 y 13 (PB4, PB5, PB6 y PB7) para el grupo 2.
    constexpr PulseOutputGroup group2{
        {&DDRB, &PORTB, B01010000},
        {&DDRB, &PORTB, B10100000}
    };

    inline void activatePin(const PulseOutputPin& pin)
    {
        *pin.outputRegister |= pin.bitMask;
    }

    inline void deactivatePin(const PulseOutputPin& pin)
    {
        *pin.outputRegister &= static_cast<unsigned char>(~pin.bitMask);
    }

    inline void runGroup(const PulseOutputGroup& group, const TimingConfiguration& config)
    {
        const int delayMicrosecondsValue =
            (config.carrierFrequencyMicroseconds.value -
             (2 * config.interPeakDelayMicroseconds.value)) / 2;

        for (int i = 0; i < config.pulsesPerCycle.value; ++i)
        {
            deactivatePin(group.negative);
            activatePin(group.positive);
            delayMicroseconds(delayMicrosecondsValue);

            deactivatePin(group.positive);
            delayMicroseconds(config.interPeakDelayMicroseconds.value);

            activatePin(group.negative);
            delayMicroseconds(delayMicrosecondsValue);
        }

        deactivatePin(group.positive);
        deactivatePin(group.negative);
    }

    inline void runSynchronizedGroups(const TimingConfiguration& config)
    {
        const int delayMicrosecondsValue =
            (config.carrierFrequencyMicroseconds.value -
             (2 * config.interPeakDelayMicroseconds.value)) / 2;

        for (int i = 0; i < config.pulsesPerCycle.value; ++i)
        {
            deactivatePin(group1.negative);
            deactivatePin(group2.negative);
            activatePin(group1.positive);
            activatePin(group2.positive);

            delayMicroseconds(delayMicrosecondsValue);

            deactivatePin(group1.positive);
            deactivatePin(group2.positive);
            delayMicroseconds(config.interPeakDelayMicroseconds.value);

            activatePin(group1.negative);
            activatePin(group2.negative);
            delayMicroseconds(delayMicrosecondsValue);
        }

        deactivatePin(group1.positive);
        deactivatePin(group1.negative);
        deactivatePin(group2.positive);
        deactivatePin(group2.negative);
    }

    inline void disablePulseOutputs()
    {
        deactivatePin(group1.positive);
        deactivatePin(group1.negative);
        deactivatePin(group2.positive);
        deactivatePin(group2.negative);
    }

    inline void runAsymmetricGroups(const TimingConfiguration& config)
    {
        // El patrón asimétrico se definirá posteriormente.
        disablePulseOutputs();
        delay(1000 / config.frequencyHz.value);
    }
}

void initializePulseOutputs()
{
    *group1.positive.directionRegister |= group1.positive.bitMask;
    *group1.negative.directionRegister |= group1.negative.bitMask;
    *group2.positive.directionRegister |= group2.positive.bitMask;
    *group2.negative.directionRegister |= group2.negative.bitMask;
}

void runPulseOutputs(const TimingConfiguration& config)
{
    if (config.state.value == kStateOff)
    {
        disablePulseOutputs();
        return;
    }

    switch (config.symmetry.value)
    {
        case kSymmetryS:
            runSynchronizedGroups(config);
            delay(1000 / config.frequencyHz.value);
            return;

        case kSymmetryA:
            runAsymmetricGroups(config);
            return;

        case kSymmetryR:
        default:
            break;
    }

    runGroup(group1, config);
    delayMicroseconds(config.groupDelayMilliseconds.value);

    const unsigned long group2StartMicroseconds = micros();
    runGroup(group2, config);

    const unsigned long group2DurationMicroseconds = micros() - group2StartMicroseconds;
    const unsigned long periodMicroseconds =
        1000000UL / config.frequencyHz.value;

    if (group2DurationMicroseconds < periodMicroseconds)
        delayMicroseconds(periodMicroseconds - group2DurationMicroseconds);
}