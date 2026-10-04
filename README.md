# BuckBoost

Firmware PlatformIO para Arduino Mega 2560 (ATmega2560, 16 MHz) que genera
señales de salida por acceso directo a registros AVR. Incluye configuración por
teclado, pantalla LCD I2C y guardado de la configuración en EEPROM.

## Compilar

Desde la raíz del repositorio:

```sh
~/.platformio/penv/bin/pio run --environment megaatmega2560
```

El proyecto usa `-O3 -flto` en `platformio.ini`. No cambies estas opciones sin
medir el efecto en el tamaño y el tiempo de ejecución. No hay pruebas
automatizadas; la compilación no verifica la forma de onda ni su temporización
en la placa.

## Hardware y pines

| Uso | Pines Mega | Registros y bits |
| --- | --- | --- |
| Grupo 1, fase positiva | 6 y 8 | `PORTH`, `PH3` y `PH5` |
| Grupo 1, fase negativa | 7 y 9 | `PORTH`, `PH4` y `PH6` |
| Grupo 2, fase positiva | 10 y 12 | `PORTB`, `PB4` y `PB6` |
| Grupo 2, fase negativa | 11 y 13 | `PORTB`, `PB5` y `PB7` |
| LCD I2C PCF8574, dirección `0x27` | 20 (SDA), 21 (SCL) | `Wire` |
| Teclado matricial 1×5 | fila 35; columnas 30–34 | `PC2`; `PC7`–`PC3` |

Las máscaras de salida están definidas en `src/pulse_outputs.cpp`:

```text
Grupo 1: positiva B00101000, negativa B01010000
Grupo 2: positiva B01010000, negativa B10100000
```

Los pines 0 y 1 corresponden a `Serial0` y al USB de la Mega. El firmware llama
a `Serial.begin(115200)`, aunque actualmente no escribe mensajes serie. No los
reasignes sin comprobar el impacto en la conexión USB.

## Configuración y manejo

Al arrancar se inicializan las salidas, se carga una configuración EEPROM válida
si existe y se muestra el resumen en la LCD. Sin una firma y versión EEPROM
reconocidas se conservan los valores iniciales definidos en `src/main.cpp`.

Los campos configurables son:

| Campo | Rango | Unidad / significado |
| --- | ---: | --- |
| Estado | 0–1 | apagado / encendido |
| Frecuencia | 1–100 | Hz |
| Frecuencia portadora | 1–999 | µs |
| Pulsos por ciclo | 1–999 | pulsos |
| Retardo entre picos | 0–100 | µs |
| Simetría | `R`, `S`, `A` | modo de agrupación |
| Retardo de grupo | 0–9999 | unidades de 10 µs |

Cuando el menú está cerrado, cualquier tecla lo abre. Dentro del menú:

| Tecla | Acción |
| --- | --- |
| `N` | Siguiente campo |
| `B` | Campo anterior |
| `I` | Incrementar |
| `D` | Decrementar |
| `S` | Guardar la configuración |

El menú se cierra tras 6 segundos sin interacción. Los cambios se editan en un
valor temporal; al cambiar de campo se descarta la edición no guardada. Si se
rechaza una configuración, se detienen las salidas, el estado en RAM pasa a
apagado y la LCD muestra el error durante 5 segundos; la EEPROM no se modifica.

Los modos de simetría implementados en `src/pulse_outputs.cpp` son:

- `R`: grupo 1, retardo de grupo, grupo 2 y espera hasta el siguiente periodo.
- `S`: ambos grupos sincronizados y espera de sincronización.
- `A`: grupo 1, mitad de periodo, grupo 2 y otra mitad de periodo.

El cálculo de fases y periodos usa división entera en microsegundos, por lo que
puede truncar fracciones.

## Implementación y limitaciones de temporización

`src/pulse_outputs.cpp` conmuta directamente los registros de puerto y ejecuta
los ciclos de forma bloqueante. El teclado se atiende después de cada ciclo de
salida; por tanto, la respuesta al teclado depende de la duración del ciclo.

La implementación actual de `delayPreciseMicroseconds()` está inline en
`include/precise_delay.h`; `src/precise_delay.cpp` solo incluye esa cabecera.
Para retardos de hasta 100 µs usa `delayMicroseconds()` del core. Para retardos
mayores, usa Timer1 en modo CTC con prescaler 8, en bloques de hasta 30 000 µs.
La función configura y detiene Timer1, sin restaurar su configuración previa.
Por ello, Timer1 queda reservado y puede entrar en conflicto con otras funciones
que lo utilicen (por ejemplo, PWM o bibliotecas de temporizadores).

La implementación no debe considerarse una garantía de precisión sin medición
en el hardware. Además, aunque el comentario de la cabecera dice que ignora los
valores no positivos, la rama de hasta 100 µs los pasa a `delayMicroseconds()`;
no se deben pasar retardos negativos a esta función. `runPulseOutputs()` valida
los tiempos derivados antes de iniciar un ciclo, pero cualquier nuevo punto de
llamada debe preservar esa protección.

## Organización

- `src/main.cpp`: inicialización y ciclo principal.
- `src/pulse_outputs.cpp`: configuración y conmutación de los grupos de salida.
- `include/precise_delay.h`: implementación inline de la espera actual.
- `src/precise_delay.cpp`: unidad de compilación que incluye la cabecera; no
  contiene la implementación.
- `src/configuration_menu.cpp`: edición, validación y persistencia EEPROM.
- `src/lcd_display.cpp` y `src/keypad_config.cpp`: LCD y teclado.
- `include/`: cabeceras públicas del firmware.

La configuración EEPROM se almacena como estructura binaria con firma
`0x4254` y versión `3`. Si se cambia su estructura o el orden de sus campos, hay
que revisar y actualizar la versión.

## Referencia

El enfoque de manipulación directa de puertos está inspirado en el proyecto
[OpenVstim](https://github.com/MonzurulAlam/OpenVstim/blob/main/README.md).
