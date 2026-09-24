# BuckBoost

BuckBoost is a compact Arduino project focused on high-speed digital output control using direct port manipulation on an Arduino Mega 2560.

The implementation is inspired by and based on the work described in the following reference project:

https://github.com/MonzurulAlam/OpenVstim/blob/main/README.md

This project follows the same general approach of optimizing pin operation by bypassing slower standard Arduino functions and writing directly to the MCU registers. That makes it suitable for timing-sensitive applications where performance matters more than readability.

## Purpose

The project demonstrates how to use hardware-level register access to:

- configure pins faster than using `pinMode()`
- toggle outputs with reduced overhead using direct port writes
- achieve lower-latency behavior in time-critical tasks

## Arduino Mega 2560 port mapping

The following table maps each digital pin to its corresponding AVR port and bit:

| Digital pin | AVR port | Digital pin | AVR port | Digital pin | AVR port |
| ---: | :--- | ---: | :--- | ---: | :--- |
| 0 | PE0 | 18 | PD3 | 36 | PC1 |
| 1 | PE1 | 19 | PD2 | 37 | PC0 |
| 2 | PE4 | 20 | PD1 | 38 | PD7 |
| 3 | PE5 | 21 | PD0 | 39 | PG2 |
| 4 | PG5 | 22 | PA0 | 40 | PG1 |
| 5 | PE3 | 23 | PA1 | 41 | PG0 |
| 6 | PH3 | 24 | PA2 | 42 | PL7 |
| 7 | PH4 | 25 | PA3 | 43 | PL6 |
| 8 | PH5 | 26 | PA4 | 44 | PL5 |
| 9 | PH6 | 27 | PA5 | 45 | PL4 |
| 10 | PB4 | 28 | PA6 | 46 | PL3 |
| 11 | PB5 | 29 | PA7 | 47 | PL2 |
| 12 | PB6 | 30 | PC7 | 48 | PL1 |
| 13 | PB7 | 31 | PC6 | 49 | PL0 |
| 14 | PJ1 | 32 | PC5 | 50 | PB3 (MISO) |
| 15 | PJ0 | 33 | PC4 | 51 | PB2 (MOSI) |
| 16 | PH1 | 34 | PC3 | 52 | PB1 (SCK) |
| 17 | PH0 | 35 | PC2 | 53 | PB0 (SS) |

## Pulse output groups

The pulse outputs use two pins per phase. Each mask is written directly to the
corresponding AVR port, so unrelated port bits are preserved by the firmware.

| Group | Phase | Digital pins | AVR bits | Direction register | Output register |
| :--- | :--- | :--- | :--- | :--- | :--- |
| 1 | Positive | 0 and 2 | `PE0` and `PE4` | `DDRE` | `PORTE` |
| 1 | Negative | 1 and 3 | `PE1` and `PE5` | `DDRE` | `PORTE` |
| 2 | Positive | 6 and 8 | `PH3` and `PH5` | `DDRH` | `PORTH` |
| 2 | Negative | 7 and 9 | `PH4` and `PH6` | `DDRH` | `PORTH` |

The masks for the active groups are:

```cpp
// Group 1: pins 0/2 and 1/3
{B00010001, B00100010}

// Group 2: pins 6/8 and 7/9
{B00101000, B01010000}
```

Pins 0 and 1 are `Serial0` RX/TX and are also connected to the Mega USB
interface. Do not use them for pulse output together with USB serial
communication or serial debugging.

## Notes

This kind of optimization is commonly used in embedded systems where execution speed is critical, such as signal generation, timing control, or fast output patterns. The code is intentionally low-level and should be understood carefully before being reused in production projects.

## Reference

This project is based on the concepts and implementation patterns described in the work referenced above:

- OpenVstim reference: https://github.com/MonzurulAlam/OpenVstim/blob/main/README.md
