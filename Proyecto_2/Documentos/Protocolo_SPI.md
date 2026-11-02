# Protocolo SPI ESP32-S3 (esclavo) ↔ Raspberry Pi 4 (maestro)

Contrato entre `Codigo/Firmware_ESP32S3/src/protocol.[ch]` y `Codigo/Software_RPi/src/protocol.[ch]`.

## 1. Capa física

| Parámetro | Valor |
|---|---|
| Modo SPI | 0 (CPOL = 0, CPHA = 0), MSB primero, 8 bits por palabra |
| Reloj | Lo fija el maestro. Diseño: 8 MHz de arranque, objetivo 15,6 – 20 MHz (ver nota) |
| Tensión | 3,3 V |
| Transacción | **64 bytes fijos** (una activación de CS). Full duplex: MISO lleva un segmento de datos, MOSI lleva un comando |
| Orden de bytes | Little-endian para enteros de 16/32 bits |
| CRC | CRC-16/CCITT-FALSE: polinomio 0x1021, valor inicial 0xFFFF, sin reflexión, XOR final 0. Vector de prueba: `"123456789"` → `0x29B1`. Se transmite en little-endian |

**¿Por qué 64 bytes?** En el ESP32-S3 el driver SPI esclavo de Zephyr 4.5 (`spi_esp32_spim`) usa el FIFO del periférico (no DMA), y el FIFO admite 64 bytes por activación de CS. Una ventana completa no cabe en una transacción, así que se reparte en 5 segmentos.

> **Nota sobre la velocidad.** `spidev` redondea el reloj pedido al divisor disponible. Con `core_freq=500` MHz, SPI0 de la Raspberry Pi 4 ofrece 31,25 / 15,625 / 7,8125 MHz, por lo que pedir 20 MHz produce 15,625 MHz. La velocidad real se mide con analizador lógico (prueba IT-03…IT-06).

## 2. Segmento ESP32 → RPi (MISO, 64 bytes)

| Offset | Tamaño | Campo | Descripción |
|---|---|---|---|
| 0 | 1 | `SYNC` | `0xA5` |
| 1 | 1 | `IDX` | Índice de segmento `0..4`; `0xFF` = IDLE (aún no hay ventana) |
| 2 | 2 | `WSEQ` | Contador de ventanas adquiridas (se desborda en 65 536). Los saltos indican ventanas no leídas |
| 4 | 1 | `PRESET` | Preset con el que se capturó la ventana (0..3) |
| 5 | 1 | `CMD_ACK` | `cmd_seq` del último comando ejecutado |
| 6 | 56 | `PAYLOAD` | Fragmento `IDX` del *stream* de la ventana |
| 62 | 2 | `CRC16` | CRC de los bytes 0..61 |

### Stream de la ventana (280 bytes = 5 × 56)

| Offset | Tamaño | Campo |
|---|---|---|
| 0 | 4 | `fs_hz` – frecuencia de muestreo nominal por canal (Sa/s) |
| 4 | 2 | `n` – muestras por canal (64) |
| 6 | 1 | `n_ch` – canales (2) |
| 7 | 1 | `bits` – resolución del ADC (12) |
| 8 | 256 | Datos: 64 pares `CH1[i], CH2[i]`, cada muestra `uint16` (0…4095) |
| 264 | 16 | Relleno (ceros) |

Un ciclo de lectura completo son 5 transacciones (`IDX` = 0,1,2,3,4) que pertenecen a la **misma** ventana: el ESP32 congela el slot de la ventana más reciente al empezar el ciclo y no lo reemplaza hasta entregar los 5 segmentos.

## 3. Comando RPi → ESP32 (MOSI, 64 bytes; solo los 8 primeros se usan)

| Offset | Tamaño | Campo | Descripción |
|---|---|---|---|
| 0 | 1 | `SYNC` | `0x5A` |
| 1 | 1 | `CMD` | `0x00` NOP · `0x01` SET_PRESET · `0x02` RESYNC |
| 2 | 1 | `ARG` | Para SET_PRESET: 0 = 100, 1 = 1 000, 2 = 10 000, 3 = 41 667 Sa/s |
| 3 | 1 | `CMD_SEQ` | Número de comando (1..255, nunca 0). El ESP32 ejecuta un comando solo si `CMD_SEQ` ≠ último ejecutado |
| 4 | 2 | reservado | 0 |
| 6 | 2 | `CRC16` | CRC de los bytes 0..5 |

**Entrega fiable del comando.** La RPi repite el comando en cada transacción hasta ver `CMD_ACK == CMD_SEQ` en un segmento recibido; después envía NOP. Como el esclavo ignora un `CMD_SEQ` repetido, la repetición es idempotente. Al iniciar, la RPi adopta como `CMD_SEQ` inicial el `CMD_ACK` que informa el ESP32.

El cambio de preset se aplica en la **siguiente ventana** (latencia ≤ 1 ventana + 5 segmentos en cola).

## 4. Sincronización y recuperación

* El esclavo no puede iniciar transferencias ni avisar «dato listo»: avanza `IDX` después de cada transacción de 64 bytes completa (`IDX = (IDX + 1) mod 5`).
* La RPi aplica **tiempo de guarda** (`--gap-us`, 150 µs por defecto) entre transacciones para que el ESP32 se rearme antes del siguiente flanco de CS.
* La RPi ensambla solo si recibe `IDX` consecutivos con igual `WSEQ`; ante CRC/sync inválido o `IDX` inesperado descarta lo ensamblado y espera el siguiente `IDX = 0` (máximo 4 transacciones perdidas).
* `RESYNC` reinicia el puntero del esclavo en `IDX = 0` (disponible; el daemon lo usa solo manualmente).
* Una ventana repetida (mismo `WSEQ`) se cuenta como *duplicada* y no se vuelve a publicar.

## 5. Presets

| ID | `fs` por canal | Modo de adquisición | Intervalo entre conversiones |
|---|---|---|---|
| 0 | 100 Sa/s | Temporizador absoluto (1 par cada 10 ms) | — |
| 1 | 1 000 Sa/s | Ráfaga DMA, 64 pares | 500 µs |
| 2 | 10 000 Sa/s | Ráfaga DMA, 64 pares | 50 µs |
| 3 | 41 667 Sa/s (bonificación) | Ráfaga DMA, 64 pares | 12 µs (83 333 conversiones/s) |

Los dos canales se convierten consecutivamente dentro de cada par: CH2 se muestrea un intervalo de conversión después de CH1 (12 µs a 41,7 kSa/s, 500 µs a 1 kSa/s). El desfase es constante y conocido, y es consecuencia de que el ESP32-S3 tiene un único SAR-ADC por unidad (ADC1) multiplexado.
