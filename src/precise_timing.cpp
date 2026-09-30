#include <Arduino.h>

#include "precise_timing.h"

// Base de tiempos de 32 bits sobre el Timer1
// -----------------------------------------
// El core solo ofrece micros(), con 4 us de granularidad y sobre el Timer0
// que ya usa millis(). Para medir el hueco entre trenes a razon de 1 us hace
// falta un reloj propio.
//
// El Timer1 es un temporizador de 16 bits libre en este proyecto. Sus
// prescalers disponibles son /1, /8, /64, /128, /256 y /1024: NO hay /16. Con
// CS11 el prescaler es /8, de modo que una cuenta de TCNT1 dura 0.5 us, y el
// desbordamiento ocurre cada 65536 * 0.5 = 32768 us, es decir cada 32.768 ms.
//
// El ISR suma ese periodo a un contador de 32 bits y ticksNow() compone
// (contador << 16) | TCNT1. El resultado son 32 bits de cuentas de 0.5 us,
// con rollover practico a los 35 minutos.
//
// Esa unidad es la que usa toda la programacion de esperas a proposito:
// pasarla a microsegundos divide el rango entre 2 y lo deja en 31 bits, con
// lo que sumar el periodo al instante se sale del tope y la resta se lee como
// negativa. Ver waitUntilPulsePeriod().
//
// OJO: tratar cada cuenta como si fuera 1 us hace correr el reloj al doble de
// velocidad, y el tren sale a la mitad del periodo pedido (50 Hz -> cada
// 10 ms). De ahi el factor 2 de kTicksPerMicrosecond.
//
// El ISR corre cada 32.768 ms, una carga de interrupcion despreciable, y solo
// incrementa un contador. ticksNow() desactiva las interrupciones mientras
// compone la lectura para que el contador y TCNT1 no puedan quedar de fases
// distintas.
//
// Por que no sirve delayMicroseconds() del core para este proyecto
// -----------------------------------------------------------------
// El core AVR implementa delayMicroseconds() asi en wiring.c (rama de 16 MHz):
//
//     if (us <= 1) return;
//     us <<= 2;                       // <-- 'us' es 'unsigned int'
//     us -= 5;
//     __asm__ __volatile__ ("1: sbiw %0,1"
//                            "brne 1b" : "=w" (us) : "0" (us));
//
// El parametro es 'unsigned int', que en AVR son 16 bits, asi que el
// desplazamiento por la izquierda se queda en un registro de 16 bits. El
// contador del bucle tambien es de 16 bits, de modo que el retardo maximo
// alcanzable son 65535 / 4 = 16383 us, y por encima de 16384 us el valor
// pedido se reduce a (us mod 16384) sin error ni aviso.
//
// Confirmado en el firmware compilado: el core se integra en main() como
//
//     lds r18, 0x0471        ; config.derived.pulsePhaseMicroseconds (16 bits)
//     lds r19, 0x0472
//     movw r26, r18
//     add r26, r26
//     adc  r27, r27
//     add r26, r26           ; <<= 2 sobre un par de 16 bits: desborda aqui
//     adc  r27, r27
//     sbiw r26, 0x05         ; -= 5
//     ...
//     1: sbiw r30, 0x01
//        brne 1b
//
// Ejemplo real del fallo: a 30 Hz el retardo de sincronismo vale 32333 us,
// el kernel lo ejecuta como 32333 mod 16384 = 15949 us y el tren aparece
// cada 16.947 ms en lugar de cada 33.333 ms.

namespace
{
    // Maximo que delayMicroseconds() resuelve sin desbordar. El kernel
    // descuenta 5 ciclos de compensacion, de ahi el -1.
    constexpr unsigned int kMaxMicrosecondsPerCall = 16383;

    // El prescaler /8 hace que una cuenta de TCNT1 dure 0.5 us, luego el reloj
    // va al doble de rapido que el conteo de cuentas y hay que dividir entre 2
    // para obtener microsegundos reales.
    constexpr uint32_t kTicksPerMicrosecond = 2;

    // Paso de la ronda gruesa de waitUntilPulsePeriod(). Grande frente a la
    // resolucion del reloj, para no releerlo en cada cuenta.
    constexpr uint32_t kCoarseStepMicroseconds = 250;

    // Ventana final: por debajo de esto se gira el bus sin dormir, que es lo
    // que fija la precision final del instante.
    constexpr int32_t kSpinWindowTicks = 100 * static_cast<int32_t>(kTicksPerMicrosecond);

    // Desbordamientos de 16 bits del Timer1 acumulados. Cada uno son 32768 us.
    volatile uint32_t s_overflowCount = 0;
}

ISR(TIMER1_OVF_vect)
{
    ++s_overflowCount;
}

void initializeTimebase()
{
    s_overflowCount = 0;

    // Timer1 parado y sin comparadores: solo modo normal con prescaler /8.
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;
    TIFR1 = _BV(TOV1);
    TIMSK1 = _BV(TOIE1);
    TCCR1B = _BV(CS11); // clk/8 -> 0.5 us por cuenta (ver kTicksPerMicrosecond)
}

static uint32_t ticksNow()
{
    uint32_t high;
    uint16_t low;

    for (;;)
    {
        noInterrupts();
        high = s_overflowCount;
        low = TCNT1;

        // Si el Timer1 desbordo entre las dos lecturas, la parte alta ya no
        // corresponde a la parte baja: se repite la composicion.
        if (high == s_overflowCount)
        {
            interrupts();
            break;
        }

        interrupts();
    }

    return (high << 16) | low;
}

uint32_t tickInstantNow()
{
    return ticksNow();
}

void delayMicrosecondsExact(uint32_t microseconds)
{
    // Trozos constantes: el compilador convierte esto en un contador simple.
    while (microseconds > kMaxMicrosecondsPerCall)
    {
        delayMicroseconds(kMaxMicrosecondsPerCall);
        microseconds -= kMaxMicrosecondsPerCall;
    }

    delayMicroseconds(static_cast<unsigned int>(microseconds));
}

void waitUntilPulsePeriod(uint32_t startInstant, uint32_t periodMicroseconds)
{
    // La suma va en el dominio de cuentas, que es modulo 2^32 igual que el
    // reloj. En microsegundos el objetivo se pasaria del tope de 31 bits que
    // tiene el rango util y la resta de mas abajo se leeria como negativa al
    // dar la vuelta el reloj: el tren se adelantaria de golpe.
    const uint32_t target = startInstant + periodMicroseconds * kTicksPerMicrosecond;

    for (;;)
    {
        // La resta sin signo da la distancia correcta aunque el reloj haya dado
        // la vuelta, y el casting a con signo la vuelve negativa si ya se
        // alcanzo el objetivo.
        const int32_t remaining = static_cast<int32_t>(target - ticksNow());

        if (remaining <= 0)
            return;

        if (remaining > kSpinWindowTicks)
            delayMicrosecondsExact(kCoarseStepMicroseconds);
    }
}
