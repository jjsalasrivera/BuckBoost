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

The firmware is currently configured to use the following digital pins for output generation:

| Group | Phase | Digital pins | AVR bits | Direction register | Output register |
| :--- | :--- | :--- | :--- | :--- | :--- |
| 1 | Positive | 6 and 8 | `PH3` and `PH5` | `DDRH` | `PORTH` |
| 1 | Negative | 7 and 9 | `PH4` and `PH6` | `DDRH` | `PORTH` |
| 2 | Positive | 10 and 12 | `PB4` and `PB6` | `DDRB` | `PORTB` |
| 2 | Negative | 11 and 13 | `PB5` and `PB7` | `DDRB` | `PORTB` |

These are the active masks currently defined in the firmware:

```cpp
// Group 1: pins 6/8 and 7/9
{B00101000, B01010000}

// Group 2: pins 10/12 and 11/13
{B01010000, B10100000}
```

This means the output channels in use are:

- `PH3`, `PH4`, `PH5`, `PH6` -> pins 6, 7, 8, 9
- `PB4`, `PB5`, `PB6`, `PB7` -> pins 10, 11, 12, 13

Pins 0 and 1 are `Serial0` RX/TX and are connected to the Mega USB interface, so they are intentionally not used for pulse output in this configuration.

## Timing architecture

The firmware does not ask "how long should I wait?". It asks "when should the next
train leave?". Each cycle remembers the instant the current train started, and
the gap between trains is measured against an absolute target of
`trainStart + period`.

That only works if the chip has a trustworthy clock, so BuckBoost builds its own
instead of using `micros()`, which has 4 us of granularity and shares a timer
with `millis()`. The timebase is Timer1 in normal mode with a `/8` prescaler, so
one count is 0.5 us. A 16-bit overflow interrupt accumulates those counts into a
32-bit value, which gives a useful range of about 35 minutes. `tickInstantNow()`
reads that value and `waitUntilPulsePeriod()` sleeps until it reaches a given
instant plus a given period.

Consequences of using absolute instants instead of chained waits:

- Code that runs between trains (keypad scanning, recomputing timings) is
  absorbed by the gap itself, so the error does not accumulate from cycle to
  cycle.
- The period is exactly what was configured, at any frequency.

Three details worth knowing before touching this code:

- `delayMicroseconds()` from the Arduino core cannot be trusted here. On AVR it
  shifts a 16-bit value left by two, so any delay above 16383 us silently
  becomes `us mod 16384`. Use `delayMicrosecondsExact()` instead.
- The `/8` prescaler is not a guess. Timer1 on this chip has no `/16`, and the
  `Servo` library sets the very same `CS11` bit for the same reason, to get
  0.5 us counts. Treating a count as a whole microsecond runs the clock at double
  speed, and every train then comes out at half the period asked for.
- Scheduling happens in timer counts, not microseconds, and that matters. The
  clock spans 32 bits of counts, but converting those counts to microseconds
  halves the range to 31 bits (about 35 minutes). Scheduling a wait on a 31-bit
  value lets the target plus the period run past the end of the clock, and the
  subtraction used to measure the remaining time then reads as negative: the
  wait returns immediately and one train in every 35-minute lap comes out with
  no gap after it at all. `tickInstantNow()` and `waitUntilPulsePeriod()` work in
  counts, where the modulus is the same 2^32 the clock uses, so the wrap is
  harmless. Use that pair to schedule anything.
- The timing correction lives in `src/precise_timing.cpp` and is only used by
  the pulse generator.

## Hardware timers are a shared resource

### The idea, without the jargon

The microcontroller has six hardware timers. A timer is a clock that runs on its
own inside the chip, driven by the crystal. It counts on its own and interrupts
the program every time it wraps around.

Here is the important part: **a timer can only be configured by one thing at a
time.** There is no rule written into the chip that stops two pieces of code from
fighting over the same timer. The last one to run simply wins, and the first one
is left waiting for a moment that will never arrive.

Think of the six timers as six clocks on the wall of a room, and each part of
the program as a person sitting at a desk. Each person owns one clock and sets
it to whatever speed they need. If two people reach for the same clock, one of
them silently ends up with the wrong time, and neither gets a warning.

This project is unusually sensitive to that, because the pulse train is
scheduled against absolute instants. If the clock changes speed, every scheduled
instant in the program becomes wrong at once.

### Who owns what

| Timer | Bits | Used by | Status |
| :--- | :---: | :--- | :--- |
| Timer0 | 8 | Arduino core: `millis()`, `micros()`, `delay()`, `pulseIn()` | **Do not reconfigure it.** BuckBoost leaves it alone. |
| Timer1 | 16 | BuckBoost timebase, in `initializeTimebase()` | **Owned by BuckBoost. Do not reconfigure it.** |
| Timer2 | 8 | `tone()`, one channel | Free. Its interrupt runs while a tone plays. |
| Timer3 | 16 | `Servo`, from the 25th servo on | Free |
| Timer4 | 16 | `Servo`, from the 37th servo on | Free |
| Timer5 | 16 | `Servo`, for the first 12 servos | Free |

Only Timer0 and Timer1 are spoken for, and Timer2 to 5 are all available. Whether
a timer is *usable* still depends on which pins it can reach, and that is where it
gets tricky here.

### Every pulse pin is also a timer pin

This is the part that surprises people. On this chip each timer is wired to
specific physical pins, and the eight pins BuckBoost uses for pulse output all
happen to belong to a timer:

| Pulse pin | Pulse group | Timer channel that also drives it |
| ---: | :--- | :--- |
| 6 | 1 positive | Timer4 channel A |
| 7 | 1 negative | Timer4 channel B |
| 8 | 1 positive | Timer4 channel C |
| 9 | 1 negative | Timer2 channel B |
| 10 | 2 positive | Timer2 channel A |
| 11 | 2 negative | Timer1 channel A |
| 12 | 2 positive | Timer1 channel B |
| 13 | 2 negative | Timer0 channel A |

So a library can be dangerous in two different ways. It can **take over a pin**
that the pulse train needs, or it can **reconfigure a timer** that the timebase
depends on. On this board the first is much more common than the second, and the
first is the harder one to notice, because the code that grabs the pin has no idea
something else is already using it.

### What breaks, and how it shows up

The honest result of checking the core on this board: **nothing in the Arduino
core changes the speed of Timer1.** `analogWrite()` on pins 11 and 12 sets only
the compare-output bits and never touches the prescaler. `tone()` uses Timer2.
The clock rate turns out to be surprisingly hard to break by accident.

What does break is the pin. And there is exactly one library that breaks the
clock.

| If you use | It grabs | What you will notice |
| :--- | :--- | :--- |
| `Servo`, 13 servos or more | Timer1 | The `Servo` library takes timers in banks of 12. Servos 1 to 12 run on Timer5 and leave the timebase alone. Servo 13 onwards seizes Timer1, and its setup zeroes `TCNT1`. The clock then reads up to 32 ms in the past, so `waitUntilPulsePeriod()` spins for that long, the pulse train stalls, and when it restarts it is out of phase. Its interrupt also fires about every 2 ms, which adds jitter. |
| `Servo` on a pulse pin | That pin, whichever one you pass | `attach()` takes any pin you give it and toggles it from a timer interrupt, so the servo pulses and the BuckBoost pulses overwrite each other. There are no default servo pins. |
| `tone()` on pins 6 to 13 | That pin, whichever one you pass | The timebase is untouched. The tone and the pulse train overwrite each other, because `tone()` toggles the pin from the Timer2 interrupt. |
| `tone()` on any other pin | Timer2 only | Works. The Timer2 interrupt is short, so expect about 1 us of jitter on a pulse edge while a tone is playing. |
| `analogWrite()` on pins 11 or 12 | That pin only | This is the one that looks harmless, and the timebase is in fact **not** affected: `analogWrite()` sets the compare-output bit in `TCCR1A` and leaves the prescaler alone. But once that bit is set the hardware drives the pin, so the group 2 pulses on pins 11 and 12 are replaced by PWM. |
| `analogWrite()` on pins 4 or 13 | Timer0 | Breaks `millis()`, `micros()` and `delay()`, which the configuration menu depends on. Pin 13 is also a group 2 pulse output. |
| `analogWrite()` on pins 6, 7, 8, 9, 10 | That pin only | The timebase is not affected. The pin becomes PWM output, so that group 1 or group 2 pulse disappears. |
| Anything that disables interrupts for more than 33 ms | Timer1's overflow interrupt | The 32-bit timebase loses a count and jumps forward by 33 ms, which shows up as one long gap in the train sequence. |

### What is still safe to use

- `millis()`, `micros()`, `delay()`, `delayMicrosecondsExact()`. These use
  Timer0, which BuckBoost leaves alone.
- `digitalRead()`, `digitalWrite()`, `pinMode()` on any pin not in the pulse pin
  table above.
- `analogWrite()` on pins 2, 3 or 5, and on pins 44, 45 or 46. Those belong to
  Timer3 and Timer5, both unused, and the pins are unused too.
- `analogRead()`.
- `Wire` / I2C, `SPI`, `EEPROM`, and the `LiquidCrystal_PCF8574` display already
  in use.
- `Serial` for debugging, as long as you stay away from pins 0 and 1, which are
  the USB connection.
- `tone()`, and up to 12 servos, as long as they stay off pins 6 to 13.

### If you need a buzzer, a beep or a second waveform

`tone()` works on this board. On the Mega 2560 it drives Timer2 and toggles the
pin from that interrupt, so it does not need a hardware compare output and it
does not touch Timer1. Point it at pin 5 and you are done.

Two things to keep in mind:

- Only one tone at a time. The core gives this chip a single Timer2 slot.
- If the beep has to line up with the pulse trains, generating it inside the
  existing scheduler is the only way it will stay in step. The absolute-instant
  approach used for the pulses applies directly, and it avoids the interrupt
  jitter altogether.

For anything more elaborate, Timer3, Timer4 and Timer5 are all free and none of
them is used by the timebase. Timer3 reaches pins 2, 3 and 5, Timer4 reaches
pins 6, 7 and 8, and Timer5 reaches pins 44, 45 and 46.

### How to check any of this yourself

These tables answer every question of this kind, and none of them is a guess:

- `variants/mega/pins_arduino.h` maps each digital pin to the timer channel wired
  to it, in the `digital_pin_to_timer_PGM` array. That is where the pulse pin
  table above comes from.
- `cores/arduino/Tone.cpp` shows which timer `tone()` selects for this chip, in
  the `USE_TIMER2` and `tone_pin_to_timer_PGM` definitions. Its `initISR()`
  cases also show the prescaler it asks for, which is how you tell whether it
  would change the speed of a timer you already depend on.
- The `Servo` library has the same kind of table in `src/avr/ServoTimers.h`. For
  this chip it reads `{ _timer5, _timer1, _timer3, _timer4 }` with twelve servos
  per timer, and that order is what decides whether Timer1 is at risk.

Before adding any library that drives hardware, look up which timer it claims and
which pins it wants in its own source, then check both against the tables above.

## Notes

This kind of optimization is commonly used in embedded systems where execution speed is critical, such as signal generation, timing control, or fast output patterns. The code is intentionally low-level and should be understood carefully before being reused in production projects.

## Reference

This project is based on the concepts and implementation patterns described in the work referenced above:

- OpenVstim reference: https://github.com/MonzurulAlam/OpenVstim/blob/main/README.md
