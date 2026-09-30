#pragma once

#include <stdint.h>

// Debe llamarse una vez desde setup(), antes de usar microsNow() o
// waitUntilMicroseconds(). Configura el Timer1 como base de tiempos de
// 32 bits con resolucion de 1 us.
void initializeTimebase();

// Microsegundos desde initializeTimebase(), en 32 bits sin rollover
// practico (rango util de unos 35 minutos). El valor se compone con el
// contador de desbordamientos y TCNT1, de modo que no depende de que el
// programa lea el reloj con una cadencia concreta.
uint32_t microsNow();

// Retardo en microsegundos que no sufre el desbordamiento de 16 bits de
// delayMicroseconds() del core AVR. Admite cualquier valor de 32 bits.
//
// Sustituye a delayMicroseconds() en todo el generador de pulsos. Ver
// src/precise_timing.cpp para el detalle del fallo que corrige.
void delayMicrosecondsExact(uint32_t microseconds);

// Espera hasta el instante absoluto indicado, medido con microsNow().
//
// Es la pieza que elimina la deriva del periodo: el hueco entre dos trenes
// consecutivos se mide contra un instante absoluto, no encadenando esperas
// relativas. Asi, todo el codigo que se ejecuta entre medias (lectura de
// teclado, recomputo de tiempos derivados, conmutacion de pines) lo absorbe
// el propio hueco y el error no llega a acumularse entre ciclos.
void waitUntilMicroseconds(uint32_t targetMicroseconds);
