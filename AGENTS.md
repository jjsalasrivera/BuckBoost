# AGENTS.md

Firmware PlatformIO para Arduino Mega 2560 (ATmega2560, 16 MHz). Genera
señales digitales mediante acceso directo a registros AVR e incluye un menú
con teclado, LCD I2C y persistencia en EEPROM.

## Compilación y validación

`pio` no está en el `PATH` en este entorno. Desde la raíz del repositorio:

```sh
~/.platformio/penv/bin/pio run --environment megaatmega2560
```

Para cargar el firmware (requiere la placa conectada):

```sh
~/.platformio/penv/bin/pio run -t upload --environment megaatmega2560
```

- Especifica siempre `--environment megaatmega2560`.
- `platformio.ini` usa `-O3 -flto` y quita `-Os` deliberadamente; no reviertas
  esas opciones sin una razón medida.
- No hay pruebas automatizadas, linter ni formateador. Tras editar código,
  compila y comunica el resultado. Una compilación correcta no valida la
  temporización ni la forma de onda; los cambios de registros, temporizadores o
  tiempos deben probarse también con hardware e instrumentación.
- No declares verificada en hardware una propiedad comprobada solo mediante la
  compilación.

## Estructura

Las cabeceras están en `include/` y las implementaciones en `src/`.

- `src/main.cpp`: define las globales `lcd`, `config` y `menu`; contiene
  `setup()` y `loop()`.
- `src/pulse_outputs.cpp`: máscaras, configuración de dirección y conmutación
  directa de los pines de salida.
- `include/precise_delay.h`: implementación inline de
  `delayPreciseMicroseconds()`.
- `src/precise_delay.cpp`: actualmente solo incluye `precise_delay.h`; no
  contiene la implementación.
- `src/configuration_menu.cpp`: campos editables, cálculo y validación de
  tiempos, menú y EEPROM.
- `src/lcd_display.cpp` / `include/lcd_display.h`: LCD I2C PCF8574.
- `src/keypad_config.cpp` / `include/keypad_config.h`: teclado 1×5.
- `include/types.h`: configuración, tiempos derivados y tipos de salida.

Contrasta siempre la documentación con el código. En particular, la
implementación de temporización actual no coincide con descripciones anteriores
del proyecto que mencionaban Timer5.

## Hardware

| Uso | Pines Mega 2560 | Registros y bits |
| --- | --- | --- |
| Grupo 1 positivo | 6 y 8 | `PORTH`, `PH3` y `PH5` |
| Grupo 1 negativo | 7 y 9 | `PORTH`, `PH4` y `PH6` |
| Grupo 2 positivo | 10 y 12 | `PORTB`, `PB4` y `PB6` |
| Grupo 2 negativo | 11 y 13 | `PORTB`, `PB5` y `PB7` |
| LCD I2C, dirección `0x27` | 20 SDA, 21 SCL | `Wire` |
| Teclado 1×5 | fila 35, columnas 30–34 | `PC2`, `PC7`–`PC3` |

Los grupos y máscaras actuales están en el espacio de nombres anónimo de
`src/pulse_outputs.cpp`. Cada `PulseOutputPin` contiene punteros distintos a
`DDRx` y `PORTx`, además de una máscara. Configura los registros de dirección
antes de activar salidas y no confundas el puntero con el valor apuntado.

Los pines 0 y 1 son `Serial0` (RX/TX) y están conectados al USB de la Mega.
`setup()` ejecuta `Serial.begin(115200)`, pero el firmware actual no escribe
mensajes serie. No los reasignes sin considerar la conexión USB.

## Salidas y temporización actual

`runPulseOutputs()` es bloqueante: genera un ciclo completo de pulsos antes de
que `loop()` procese la siguiente interacción con el teclado. Los modos son:

- `R`: grupo 1 → retardo entre grupos → grupo 2 → periodo restante.
- `S`: ambos grupos sincronizados → retardo de sincronización.
- `A`: grupo 1 → mitad de periodo → grupo 2 → mitad de periodo.

La implementación de `delayPreciseMicroseconds()` se encuentra inline en
`include/precise_delay.h` para que el compilador pueda optimizar las llamadas.
Actualmente:

- Para `us <= 100`, llama a `delayMicroseconds(us)`.
- Para valores mayores, configura Timer1 en modo CTC con prescaler 8 y divide
  esperas mayores de 30 000 µs en bloques.
- Esos bloques ocupan Timer1 y no restauran su configuración anterior; no
  introduzcas otro uso de Timer1 (incluido PWM o bibliotecas de temporizadores)
  sin revisar el conflicto.
- El comentario de la cabecera dice que ignora valores no positivos, pero la
  rama `us <= 100` pasa también los valores negativos a `delayMicroseconds()`.
  No pases valores negativos. Conserva las comprobaciones de validez de
  `runPulseOutputs()` y añade una guarda si agregas nuevos puntos de llamada.
- La precisión real, la respuesta ante interrupciones y la forma de onda no
  están verificadas por la compilación. Mide en la placa antes de afirmar
  tolerancias o exactitud.

No cambies a interrupciones o temporizadores de hardware sin analizar el ciclo
bloqueante, las salidas y los periféricos que comparten temporizadores.

## Configuración, menú y EEPROM

Hay siete campos editables en `TimingConfiguration`: estado, frecuencia,
frecuencia portadora, pulsos por ciclo, retardo entre picos, simetría y retardo
de grupo. El teclado usa `N` (siguiente), `B` (anterior), `I` (incrementar),
`D` (decrementar) y `S` (guardar). El menú se cierra tras 6 segundos sin
interacción. Cambiar de campo restaura el valor de edición al confirmado; no
presupongas que una edición pendiente se conserva.

Los tiempos derivados se recalculan al validar la configuración. La propiedad
`derived` es `mutable`, de modo que la validación puede actualizarla incluso a
través de `const TimingConfiguration&`. Las fórmulas usan división entera.

La estructura EEPROM se guarda con `EEPROM.put()`; firma actual `0x4254`,
versión `3`. Si se cambia la estructura almacenada o el orden de campos, revisa
la compatibilidad y aumenta la versión. El orden de campos también está
duplicado en `include/types.h`, `getField()`, `validateFields()` y las listas
paralelas de `loadConfiguration()`; sincroniza esos lugares y el contador de
campos al añadir, quitar o reordenar uno.

`isValidField()`, `changeField()` y `LcdDisplay::print(ConfigurationField)`
reconocen los campos de simetría y estado comparando los primeros caracteres
de `field.name`. Si se cambian esos nombres, revisa también dichas
comparaciones. Los modos válidos de simetría son `R`, `S` y `A`; el estado es
0 o 1.

En `setup()`, conserva la configuración global usada por `loop()` y ejecuta
`initializePulseOutputs()` antes de `loadConfiguration()`: una configuración
EEPROM rechazada llama a `stopPulseOutputs()`.

## Convenciones

- Responde en español y conserva el estilo del archivo que edites.
- Usa acceso directo a registros cuando corresponda a la conmutación rápida;
  confirma siempre el puerto, el bit y los efectos sobre periféricos.
- Evita cambios no relacionados y no reviertas modificaciones locales del
  usuario.
- Para cambios de temporización, registros o máscaras, describe la validación
  realizada y la prueba de hardware que aún haga falta.
