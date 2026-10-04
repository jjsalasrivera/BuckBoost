#pragma once

#include <Arduino.h>

// Espera exacta de 1 a ~32700 µs, con resolución de 0,5 µs
static inline void esperaTimer1(uint16_t us)
{
    TCCR1B = 0;                        // para el timer
    TCCR1A = 0;                        // modo normal/CTC, sin salidas en pines
    TCNT1  = 0;
    OCR1A  = (us << 1) - 1;            // ticks de 0,5 µs (16 MHz / 8)
    TIFR1  = _BV(OCF1A);               // limpia el flag (se limpia escribiendo 1)
    TCCR1B = _BV(WGM12) | _BV(CS11);   // CTC, prescaler 8: arranca
    while (!(TIFR1 & _BV(OCF1A))) {}   // espera al compare match
    TCCR1B = 0;                        // para el timer
}

// Espera microsegseconds exactos. Los valores <= 0 se ignoran, de modo que un
// retardo derivado negativo nunca se convierte en una espera enorme.
inline void delayPreciseMicroseconds(long us)
{
    if (us <= 100) {                     // corto: delayMicroseconds es suficiente
        delayMicroseconds(us);
        return;
    }
    while (us > 30000L) {                // trocea las esperas muy largas
        esperaTimer1(30000U);
        us -= 30000L;
    }
    if (us > 0)
        esperaTimer1((uint16_t)us);
}