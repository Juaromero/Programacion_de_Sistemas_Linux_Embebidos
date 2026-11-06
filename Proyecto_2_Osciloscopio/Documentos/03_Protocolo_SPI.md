# Documento 3 — Especificación del protocolo SPI

**Sistema Híbrido de Adquisición Analógica mediante SPI y Dashboard Embebido en Linux**
Universidad Nacional de Colombia — Sede Manizales · Programación de sistemas Linux embebido · Noviembre de 2026

## 1. Alcance

Este documento es el contrato entre el firmware del ESP32-S3 ([`Codigo/Firmware_ESP32S3/src/protocol.h`](../Codigo/Firmware_ESP32S3/src/protocol.h)) y el servicio de la Raspberry Pi ([`Codigo/Software_RPi/src/protocol.h`](../Codigo/Software_RPi/src/protocol.h)). Cualquier cambio en el formato debe actualizar ambos programas y este documento.

## 2. Capa física

| Parámetro | Valor |
|---|---|
| Modo SPI | 0 (CPOL = 0, CPHA = 0), MSB primero, 8 bits por palabra |
| Reloj | Lo fija el maestro. Valor de servicio: **20 MHz** (verificado sin errores de CRC) |
| Tensión lógica | 3,3 V |
| Transacción | **64 bytes fijos** (una activación de CS), full duplex: por MISO viaja un segmento de datos y por MOSI un comando |
| Orden de bytes | Little-endian para enteros de 16 y 32 bits |
| CRC | CRC-16/CCITT-FALSE: polinomio 0x1021, valor inicial 0xFFFF, sin reflexión, XOR final 0. Vector de prueba: `"123456789"` → `0x29B1`. Se transmite en little-endian |
| Tiempo de guarda | 150 µs entre transacciones, para que el esclavo se rearme |

**¿Por qué 64 bytes?** En el ESP32-S3 el driver SPI esclavo de Zephyr 4.5 (`spi_esp32_spim`) usa el FIFO del periférico y no DMA, y el FIFO admite 64 bytes por activación de CS. Una ventana completa no cabe en una transacción, de modo que se reparte en cinco segmentos.

**Nota sobre la velocidad.** El kernel de la Raspberry Pi 4 acepta las velocidades pedidas a `spidev`, pero el reloj efectivo lo determina un divisor entero del reloj del núcleo del SoC, por lo que puede diferir del valor solicitado. La velocidad real se mide con analizador lógico (pruebas IT-03 a IT-06 del [Documento 2](02_Matriz_verificacion_y_plan_de_pruebas.md)).

## 3. Segmento ESP32-S3 → Raspberry Pi (MISO, 64 bytes)

| Offset | Bytes | Campo | Descripción |
|---|---|---|---|
| 0 | 1 | SYNC | `0xA5` |
| 1 | 1 | IDX | Índice de segmento 0 a 4; `0xFF` = IDLE (aún no hay ventana) |
| 2 | 2 | WSEQ | Contador de ventanas adquiridas (se desborda en 65 536); sus saltos indican ventanas no leídas |
| 4 | 1 | PRESET | Preset de CH1 con el que se capturó la ventana |
| 5 | 1 | CMD_ACK | `CMD_SEQ` del último comando ejecutado |
| 6 | 56 | PAYLOAD | Fragmento IDX del *stream* de la ventana |
| 62 | 2 | CRC16 | CRC de los bytes 0 a 61 |

### 3.1 Stream de la ventana (280 bytes = 5 × 56), versión 2

Cada canal tiene su propio preset y su propia frecuencia de muestreo, y puede estar desactivado.

| Offset | Bytes | Campo |
|---|---|---|
| 0 | 1 | Versión del formato (2) |
| 1 | 1 | Máscara de canales adquiridos: bit 0 = CH1, bit 1 = CH2 |
| 2 | 1 | Resolución del ADC (12) |
| 3 | 1 | Banderas: bit 0 = canales sincronizados (misma ráfaga) |
| 4 | 2 | `n`: muestras por canal (64) |
| 6 | 1 | Preset de CH1 (0 a 4) |
| 7 | 1 | Preset de CH2 (0 a 4) |
| 8 | 4 | Frecuencia de muestreo de CH1 (Sa/s) |
| 12 | 4 | Frecuencia de muestreo de CH2 (Sa/s) |
| 16 | 128 | CH1: 64 muestras `uint16` (0 a 4095); ceros si el canal está apagado |
| 144 | 128 | CH2: 64 muestras `uint16` |
| 272 | 8 | Relleno (ceros) |

**Compatibilidad con la versión 1.** El firmware anterior enviaba `fs` (4 bytes), `n` (2), `n_ch` = 2 (1), `bits` = 12 (1) y 64 pares CH1, CH2 intercalados. La Raspberry Pi distingue los formatos porque en la versión 1 los bytes 6 y 7 valen 2 y 12, combinación imposible en la versión 2 (presets de 0 a 4), y acepta ambos.

Un ciclo de lectura completo son cinco transacciones (IDX = 0, 1, 2, 3, 4) que pertenecen a la **misma** ventana: el ESP32-S3 congela el slot de la ventana más reciente al empezar el ciclo y no lo reemplaza hasta entregar los cinco segmentos.

## 4. Comando Raspberry Pi → ESP32-S3 (MOSI)

De los 64 bytes de MOSI solo se usan los primeros 8; el resto se ignora.

| Offset | Bytes | Campo | Descripción |
|---|---|---|---|
| 0 | 1 | SYNC | `0x5A` |
| 1 | 1 | CMD | `0x00` NOP · `0x01` SET_PRESET (ambos canales) · `0x02` RESYNC · `0x03` SET_CH_PRESET (un canal) · `0x04` SET_MASK (activar canales) |
| 2 | 1 | ARG | SET_PRESET: preset 0 a 4. SET_CH_PRESET: `(canal << 4) \| preset`, con canal 0 = CH1 y 1 = CH2. SET_MASK: bit 0 = CH1, bit 1 = CH2 |
| 3 | 1 | CMD_SEQ | Número de comando (1 a 255, nunca 0). El ESP32-S3 ejecuta un comando solo si `CMD_SEQ` difiere del último ejecutado |
| 4 | 2 | reservado | 0 |
| 6 | 2 | CRC16 | CRC de los bytes 0 a 5 |

**Entrega fiable del comando.** La Raspberry Pi repite el comando en cada transacción hasta ver `CMD_ACK == CMD_SEQ` en un segmento recibido; después envía NOP. Como el esclavo ignora un `CMD_SEQ` repetido, la repetición es idempotente. Al iniciar, la Raspberry Pi adopta como `CMD_SEQ` inicial el `CMD_ACK` que informa el ESP32-S3, y solo entonces numera los comandos pendientes. El cambio de preset se aplica en la **siguiente** ventana.

**Cola de comandos.** Los comandos se envían de a uno, en orden, desde una cola. Si un comando no se confirma en 400 transacciones (por ejemplo, porque el firmware es anterior y no lo conoce), se descarta para no bloquear los siguientes. Si el ESP32-S3 se reinicia (salto grande hacia atrás de WSEQ), la Raspberry Pi reenvía la máscara de canales y el preset de cada canal elegidos por el usuario.

## 5. Sincronización y recuperación

- El esclavo no puede iniciar transferencias ni avisar «dato listo»: avanza IDX después de cada transacción de 64 bytes completa (IDX ← (IDX + 1) mod 5).
- La Raspberry Pi aplica un **tiempo de guarda** (`--gap-us`, 150 µs) entre transacciones para que el ESP32-S3 se rearme antes del siguiente flanco de CS.
- La Raspberry Pi ensambla solo si recibe IDX consecutivos con igual WSEQ; ante CRC o sincronía inválidos, o un IDX inesperado, descarta lo ensamblado y espera el siguiente IDX = 0 (máximo cuatro transacciones perdidas).
- RESYNC reinicia el puntero del esclavo en IDX = 0 (disponible; el servicio solo lo usa manualmente).
- Una ventana repetida (mismo WSEQ) se cuenta como *duplicada* y no se vuelve a publicar.
- La primera ventana recibida después de arrancar el servicio se descarta, porque puede ser una ventana antigua que el esclavo dejó congelada mientras nadie leía el bus.

## 6. Presets

| ID | $f_s$ por canal | Modo | Intervalo, 2 canales en ráfaga | Intervalo, 1 canal |
|---|---|---|---|---|
| 0 | 100 Sa/s | Temporizador absoluto | 10 ms por par | 10 ms |
| 1 | 1 000 Sa/s | Ráfaga DMA | 500 µs | 1 000 µs |
| 2 | 10 000 Sa/s | Ráfaga DMA | 50 µs | 100 µs |
| 3 | 41 667 Sa/s (bonificación) | Ráfaga DMA | 12 µs | 24 µs |
| 4 | 83 333 Sa/s | Ráfaga DMA | no admitido | 12 µs |

*El controlador digital del ADC admite de 611 a 83 333 conversiones por segundo.*

**Modos de adquisición:**

- **Sincronizados:** los dos canales activos con el mismo preset (0 a 3) se convierten en una sola ráfaga con el patrón CH1, CH2. CH2 se muestrea un intervalo de conversión después de CH1; el desfase es constante y conocido.
- **Independientes:** si los presets difieren, o con el preset 4, cada canal se adquiere en su propia ráfaga, uno después del otro y cada uno a su frecuencia. Sus ventanas no están alineadas en el tiempo.
- **Un canal:** el canal apagado no se adquiere, de modo que el otro se refresca más rápido; con un solo canal se alcanza el máximo del ADC, 83 333 Sa/s.

El ESP32-S3 tiene un único SAR-ADC por unidad (ADC1) multiplexado entre canales, por lo que no es posible una conversión simultánea real.
