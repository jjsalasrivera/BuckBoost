#include <Arduino.h>

#include "configuration_menu.h"
#include "precise_timing.h"
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

    inline void runGroup(const PulseOutputGroup& group, const TimingConfiguration& config, LcdDisplay& lcd )
    {
        for (int i = 0; i < config.pulsesPerCycle.value; ++i)
        {
            activatePin(group.positive);
            delayMicrosecondsExact(config.derived.pulsePhaseMicroseconds);

            deactivatePin(group.positive);
            delayMicrosecondsExact(config.interPeakDelayMicroseconds.value);

            activatePin(group.negative);
            delayMicrosecondsExact(config.derived.pulsePhaseMicroseconds);

            deactivatePin(group.negative);
            delayMicrosecondsExact(config.interPeakDelayMicroseconds.value);
        }
    }

    inline void runSynchronizedGroups(const TimingConfiguration& config, LcdDisplay& lcd)
    {
        for (int i = 0; i < config.pulsesPerCycle.value; ++i)
        {
            activatePin(group1.positive);
            activatePin(group2.positive);

            delayMicrosecondsExact(config.derived.pulsePhaseMicroseconds);

            deactivatePin(group1.positive);
            deactivatePin(group2.positive);
            delayMicrosecondsExact(config.interPeakDelayMicroseconds.value);

            activatePin(group1.negative);
            activatePin(group2.negative);
            delayMicrosecondsExact(config.derived.pulsePhaseMicroseconds);

            deactivatePin(group1.negative);
            deactivatePin(group2.negative);
            delayMicrosecondsExact(config.interPeakDelayMicroseconds.value);
        }
    }

    inline void disablePulseOutputs()
    {
        deactivatePin(group1.positive);
        deactivatePin(group1.negative);
        deactivatePin(group2.positive);
        deactivatePin(group2.negative);
    }

    inline void runAsymmetricGroups(const TimingConfiguration& config, LcdDisplay& lcd)
    {
        runGroup(group1, config, lcd);

        delayMicrosecondsExact(config.derived.asymmetricHalfPeriodMicroseconds);

        runGroup(group2, config, lcd);
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

void runPulseOutputs(const TimingConfiguration& config, LcdDisplay& lcd)
{
    // hasValidDerivedTiming recalcula config.derived y descarta cualquier
    // retardo negativo: sin este control un valor invalido se convierte en
    // un objetivo lejano para waitUntilMicroseconds y el tren se retrasa.
    if (config.state.value == kStateOff || !hasValidDerivedTiming(config))
    {
        disablePulseOutputs();
        return;
    }

    // Instante de referencia del tren. Todo el codigo que se ejecuta desde
    // aqui hasta el final del tren se descuenta del hueco, de modo que el
    // intervalo entre el primer pulso de dos trenes consecutivos es
    // exactamente el periodo de la frecuencia y la deriva no se acumula.
    const uint32_t trainStart = microsNow();
    const uint32_t period = static_cast<uint32_t>(config.derived.frequencyPeriodMicroseconds);

    switch (config.symmetry.value)
    {
        case kSymmetryS:
            runSynchronizedGroups(config, lcd);
            waitUntilMicroseconds(trainStart + period);
            return;

        case kSymmetryA:
            runAsymmetricGroups(config, lcd);
            waitUntilMicroseconds(trainStart + period);
            return;

        case kSymmetryR:
            break;
        default:
            break;
    }
    // Ejecucion con retardo entre grupos
    runGroup(group1, config, lcd);
    delayMicrosecondsExact(config.derived.groupDelayMicroseconds);
    runGroup(group2, config, lcd);
    waitUntilMicroseconds(trainStart + period);
}