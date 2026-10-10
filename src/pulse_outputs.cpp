#include <Arduino.h>

#include "configuration_menu.h"
#include "precise_delay.h"
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

    inline void runGroup(const PulseOutputGroup& group, const TimingConfiguration& config )
    {
        long remainingPulses = config.pulsesPerCycle.value;
        
        do
        {
            activatePin(group.positive);
            delayPreciseMicroseconds(config.derived.pulsePhaseMicroseconds);

            deactivatePin(group.positive);
            delayPreciseMicroseconds(config.interPeakDelayMicroseconds.value);

            activatePin(group.negative);
            delayPreciseMicroseconds(config.derived.pulsePhaseMicroseconds);

            deactivatePin(group.negative);
            delayPreciseMicroseconds(config.interPeakDelayMicroseconds.value);
        } while (--remainingPulses);
    }

    inline void runSynchronizedGroups(const TimingConfiguration& config)
    {
        long remainingPulses = config.pulsesPerCycle.value;

        do
        {
            activatePin(group1.positive);
            activatePin(group2.positive);
            delayPreciseMicroseconds(config.derived.pulsePhaseMicroseconds);

            deactivatePin(group1.positive);
            deactivatePin(group2.positive);
            delayPreciseMicroseconds(config.interPeakDelayMicroseconds.value);

            activatePin(group1.negative);
            activatePin(group2.negative);
            delayPreciseMicroseconds(config.derived.pulsePhaseMicroseconds);

            deactivatePin(group1.negative);
            deactivatePin(group2.negative);
            delayPreciseMicroseconds(config.interPeakDelayMicroseconds.value);
        } while (--remainingPulses);
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
        runGroup(group1, config);

        delayPreciseMicroseconds(config.derived.asymmetricHalfPeriodMicroseconds);

        runGroup(group2, config);

        delayPreciseMicroseconds(config.derived.asymmetricHalfPeriodMicroseconds);
    }
}

void initializePulseOutputs()
{
    *group1.positive.directionRegister |= group1.positive.bitMask;
    *group1.negative.directionRegister |= group1.negative.bitMask;
    *group2.positive.directionRegister |= group2.positive.bitMask;
    *group2.negative.directionRegister |= group2.negative.bitMask;
}

void stopPulseOutputs()
{
    disablePulseOutputs();
}

void runPulseOutputs(const TimingConfiguration& config)
{
    if (config.state.value == kStateOff || !hasValidDerivedTiming(config))
    {
        disablePulseOutputs();
        return;
    }

    switch (config.symmetry.value)
    {
        case kSymmetryS:
            runSynchronizedGroups(config);
            delayPreciseMicroseconds(config.derived.frequencyPeriodMicroseconds - config.derived.groupDurationMicroseconds);
            return;

        case kSymmetryA:
            runAsymmetricGroups(config);
            return;

        case kSymmetryR:
            break;
        default:
            break;
    }
    // Ejecucion con retardo entre grupos
    runGroup(group1, config);
    delayPreciseMicroseconds(config.derived.groupDelayMicroseconds);
    runGroup(group2, config);
    delayPreciseMicroseconds(config.derived.groupPeriodMicroseconds);
}