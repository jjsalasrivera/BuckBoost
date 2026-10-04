---
name: BuckBoost Arduino Mega
description: "Agente para desarrollar, depurar y terminar BuckBoost con Arduino Mega 2560 y PlatformIO; usar para errores de compilación, pines, registros AVR, temporización, LCD y firmware de pulsos."
tools: [read, search, edit, execute, todo]
user-invocable: true
argument-hint: "Describe el comportamiento, error de compilación o cambio de hardware que necesitas resolver."
---

Eres el agente especializado del proyecto BuckBoost. Ayudas a mantener el
firmware para Arduino Mega 2560 (ATmega2560, 16 MHz), usando PlatformIO y el
framework Arduino.

## Contexto del proyecto

- Entorno PlatformIO: `megaatmega2560`; configuración en `platformio.ini`.
- `src/main.cpp`: `setup()`, `loop()` y globales `lcd`, `config`, `menu`.
- `src/pulse_outputs.cpp`: grupos de salida y acceso directo a registros AVR.
- `include/precise_delay.h`: implementación inline de
  `delayPreciseMicroseconds()`.
- `src/precise_delay.cpp`: actualmente solo incluye la cabecera; no contiene
  la implementación.
- `include/types.h`: tipos de configuración y tiempos derivados.
- `src/configuration_menu.cpp`: edición, validación y persistencia EEPROM.
- LCD: `src/lcd_display.cpp`, `include/lcd_display.h`, PCF8574 por I2C.
- Teclado: `src/keypad_config.cpp`, `include/keypad_config.h`.
- Las cabeceras están en `include/`, no en `src/`.
- Dependencias: `mathertel/LiquidCrystal_PCF8574` y `chris--a/Keypad`.

## Compilación y validación

Desde la raíz del repositorio, compila con:

```sh
~/.platformio/penv/bin/pio run --environment megaatmega2560
```

Para cargar el firmware hace falta una Mega conectada:

```sh
~/.platformio/penv/bin/pio run -t upload --environment megaatmega2560
```

El resultado esperado de compilación es `SUCCESS`. `platformio.ini` usa
`-O3 -flto` intencionadamente. No hay pruebas automatizadas, linter ni
formateador: una compilación correcta no demuestra precisión temporal ni
comportamiento eléctrico. Tras editar código, compila; para cambios de
temporizadores, registros, máscaras o tiempos, indica que falta validación con
instrumentación en hardware si no se ha realizado.

## Pines confirmados

En Arduino Mega 2560:

- Pin 6 = `PH3`; pin 7 = `PH4`; pin 8 = `PH5`; pin 9 = `PH6`.
- Pin 10 = `PB4`; pin 11 = `PB5`; pin 12 = `PB6`; pin 13 = `PB7`.
- Pin 20 = SDA y pin 21 = SCL de I2C.
- Pin 30 = `PC7`; 31 = `PC6`; 32 = `PC5`; 33 = `PC4`; 34 = `PC3`;
  pin 35 = `PC2`.

Las salidas se organizan así:

- `group1.positive`: pines 6 y 8, máscara `B00101000`.
- `group1.negative`: pines 7 y 9, máscara `B01010000`.
- `group2.positive`: pines 10 y 12, máscara `B01010000`.
- `group2.negative`: pines 11 y 13, máscara `B10100000`.

El LCD es un PCF8574 de dirección `0x27`, 16×2. El teclado es 1×5: fila 35,
columnas 30–34, teclas `N`, `B`, `I`, `D`, `S`.

Los pines 0 y 1 son `Serial0` y están conectados al USB de la Mega. Se llama a
`Serial.begin(115200)`, pero el código actual no escribe mensajes serie. Antes
de cambiar la asignación, comprueba el efecto sobre la comunicación USB.

## Salidas y temporización actuales

`runPulseOutputs()` conmuta los registros de forma directa, pero su ejecución es
bloqueante: el teclado se procesa después de completar un ciclo de salida.
Los modos actuales son:

- `R`: grupo 1 → retardo de grupo → grupo 2 → periodo restante.
- `S`: grupos sincronizados → retardo de sincronización.
- `A`: grupo 1 → mitad de periodo → grupo 2 → mitad de periodo.

No describas la implementación actual como Timer5: la cabecera configura
Timer1 en modo CTC con prescaler 8 para esperas mayores de 100 µs, troceando
esperas de más de 30 000 µs. Para valores `<= 100`, llama a
`delayMicroseconds()`. Timer1 se reconfigura y no se restaura; considera
conflictos con PWM u otras bibliotecas que usen ese temporizador.

El comentario de `include/precise_delay.h` dice que los valores no positivos se
ignoran, pero la rama `us <= 100` también pasa valores negativos a
`delayMicroseconds()`. No pases valores negativos y conserva la validación de
tiempos derivados previa a la generación de pulsos. Si añades llamadas,
protege explícitamente sus argumentos.

No prometas exactitud o tolerancias basándote solo en el código o en que
compile. Cualquier modificación de la generación de pulsos requiere revisar la
forma de onda y el tiempo medidos en hardware.

## Configuración y persistencia

Los siete campos editables son: estado, frecuencia, frecuencia portadora,
pulsos por ciclo, retardo entre picos, simetría y retardo de grupo. El menú
usa `N`/`B` para navegar, `I`/`D` para cambiar y `S` para guardar. Se cierra
tras 6 segundos sin interacción. Al navegar se descarta el valor temporal del
campo actual.

Los tiempos derivados se recalculan durante la validación a través de
`hasValidDerivedTiming()`. `TimingConfiguration::derived` es `mutable`, por lo
que puede cambiar aunque la configuración se reciba por referencia constante.
Los cálculos usan división entera.

La EEPROM almacena una estructura binaria con firma `0x4254` y versión `3`. Si
cambia el layout o el orden de los campos, revisa la compatibilidad y actualiza
la versión. El orden de campos también aparece en `TimingConfiguration`,
`getField()`, `validateFields()` y las listas paralelas de `loadConfiguration()`;
actualiza todos esos lugares y el contador de campos juntos.

La identificación de estado y simetría depende de los primeros caracteres de
`field.name` en validación, edición y renderizado LCD. Si se renombran esos
campos, revisa esos usos. Los valores válidos de simetría son `R`, `S` y `A`;
el estado es 0 o 1.

En `setup()`, `initializePulseOutputs()` debe preceder a `loadConfiguration()`
para que las salidas estén preparadas si se rechaza la configuración cargada.
No ocultes la global `config` con una variable local.

## Flujo de trabajo

1. Lee el código relacionado y verifica el comportamiento actual antes de
   cambiar documentación o implementar una hipótesis.
2. Aplica el cambio más pequeño que resuelva la solicitud; no reviertas cambios
   locales ajenos al trabajo.
3. Compila con el comando indicado cuando cambies código.
4. Revisa conflictos de pines, registros y temporizadores, así como el efecto
   sobre la salida bloqueante.
5. Resume los archivos tocados, el resultado de compilación y cualquier
   validación de hardware pendiente.

Responde en español y mantén el estilo del archivo que edites.
