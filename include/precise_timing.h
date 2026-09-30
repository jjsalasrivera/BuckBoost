#pragma once

#include <stdint.h>

// Debe llamarse una vez desde setup(), antes de usar tickInstantNow().
// Configura el Timer1 como base de tiempos de 32 bits con resolucion de 0.5 us.
void initializeTimebase();

// Instante actual en la unidad interna del reloj (cuentas de 0.5 us), para
// pasarlo despues a waitUntilPulsePeriod().
//
// Es lo que hay que capturar al empezar un tren de pulsos. El valor no
// significa nada fuera de esta pareja de funciones: no son microsegundos.
//
// No existe un equivalente en microsegundos a proposito. El reloj abarca 32
// bits de cuentas, y al pasarlos a microsegundos el rango se reduce a 31 bits
// (unos 35 minutos). Programar una espera con un valor de 31 bits hace que la
// suma del instante mas el periodo se pase del tope y que la resta interna se
// lea como negativa, y entonces la espera retorna antes de tiempo.
uint32_t tickInstantNow();

// Retardo en microsegundos que no sufre el desbordamiento de 16 bits de
// delayMicroseconds() del core AVR. Admite cualquier valor de 32 bits.
//
// Sustituye a delayMicroseconds() en todo el generador de pulsos. Ver
// src/precise_timing.cpp para el detalle del fallo que corrige.
void delayMicrosecondsExact(uint32_t microseconds);

// Espera hasta que el reloj llegue a startInstant + periodMicroseconds.
// startInstant debe venir de tickInstantNow().
//
// Es la pieza que elimina la deriva del periodo: el hueco entre dos trenes
// consecutivos se mide contra un instante absoluto, no encadenando esperas
// relativas. Asi, todo el codigo que se ejecuta entre medias (lectura de
// teclado, recomputo de tiempos derivados, conmutacion de pines) lo absorbe
// el propio hueco y el error no llega a acumularse entre ciclos.
//
// startInstant se mide en cuentas de 0.5 us, que es un dominio modulo 2^32,
// igual que el del reloj. Por eso el instante mas el periodo se pueden sumar y
// restar sin que la cuenta se salga del rango, y la vuelta del reloj cada 35
// minutos no afecta al resultado.
void waitUntilPulsePeriod(uint32_t startInstant, uint32_t periodMicroseconds);
