# Documento 2 — Matriz de verificación y plan de pruebas

**Sistema Híbrido de Adquisición Analógica mediante SPI y Dashboard Embebido en Linux**
Universidad Nacional de Colombia — Sede Manizales · Programación de sistemas Linux embebido · Noviembre de 2026
Integrantes y roles: ver [Documento 4](04_Declaracion_participacion.md).

**Contenido:** [1. Estrategia](#1-estrategia-de-diseño-de-experimentos) · [2. Matriz](#2-matriz-de-verificación) · [3. Casos unitarios](#3-casos-de-prueba-unitarios) · [4. Casos de integración](#4-casos-de-prueba-de-integración) · [5. Registro de resultados](#5-registro-de-resultados)

---

## 1. Estrategia de diseño de experimentos

La verificación se organiza en tres niveles. Las **pruebas unitarias del firmware** del ESP32-S3 validan la adquisición y la comunicación SPI de forma aislada. Las **pruebas unitarias de la Raspberry Pi** validan el servicio SPI y el servidor HTTP. Las **pruebas de integración** verifican el sistema completo, incluidos el refresco del dashboard y el rendimiento del bus a las frecuencias de muestreo requeridas.

Las pruebas de integración de tasa SPI se ejecutan a 100, 1 000, 10 000 y 41 667 Sa/s. Los valores teóricos de referencia provienen del [Documento 1](01_Especificacion_tecnica_y_arquitectura.md): cada ventana ocupa 320 bytes en el bus (5 segmentos de 64 bytes), es decir $t_{tx} = 2\,560 / f_{SPI}$, y el ciclo de trabajo $D$ va de 0,02 % a 8,3 % a 20 MHz (de 0,04 % a 16,7 % a 10 MHz).

### 1.1 Ejecución por capas

Las pruebas se ejecutan en este orden, sin pasar a la capa siguiente hasta aprobar la anterior, porque cada una asume que la anterior funciona:

1. **ESP32-S3 aislado** (solo monitor serie): presets, duración de la ventana, desfase de canales. No requiere la Raspberry Pi.
2. **Calibración del ADC con señales conocidas:** 0 V (GND), 3,3 V y un divisor resistivo en cada canal para fijar ganancia y offset; después una señal periódica de frecuencia conocida para medir la $f_s$ real a partir de su período.
3. **Raspberry Pi aislada:** pruebas unitarias del protocolo (`make test`), configuración de `spidev`, servicio y servidor HTTP con ventanas sintéticas (`oscd --demo`).
4. **Enlace SPI:** `oscd --bench` desde 1 MHz hacia arriba, registrando errores de CRC y tiempos; ajuste de `--gap-us` al mínimo estable; medición con analizador lógico en SCLK y CS.
5. **Integración:** refresco del dashboard, cambio de preset y de canales desde la interfaz, y estabilidad de 5 minutos.

### 1.2 Instrumentación y registro

- **Analizador lógico** (8 canales, 24 MS/s o más, con `sigrok`/PulseView) en SCLK, CS y MISO: mide $t_{tx}$, el tiempo de guarda efectivo y la frecuencia real del reloj. Para 15,6–20 MHz se recomiendan al menos 100 MS/s; con 24 MS/s se mide el período de CS y el número de flancos, no la forma del reloj.
- **Generador de funciones** para verificar $f_s$ y ganancia. Alternativa sin instrumento: la Raspberry Pi genera una onda cuadrada con `gpioset -c gpiochip0 -t <periodo> GPIO18=1` en su pin 12.
- **Software:** `oscd --bench N` (tiempos por transacción, ventanas perdidas, errores), `curl` para la API y el JSON, herramientas de desarrollo del navegador para los intervalos de refresco, tecla `d` del monitor serie (diagnóstico de la ráfaga DMA).
- **Registro:** cada prueba guarda su evidencia en [`Resultados_Pruebas/`](Resultados_Pruebas/) con el identificador de la prueba, y se actualiza la columna *Estado* de la matriz.

### 1.3 Alcance de las pruebas

| Nivel | Qué se verifica |
|---|---|
| Unitarias ESP32-S3 | Duración de la ventana y $f_s$ efectiva por preset; conversión del ADC y desfase CH1/CH2; llenado de 64 muestras por canal y anillo de tres slots; segmento SPI (encabezado, datos y CRC-16); ejecución de comandos |
| Unitarias Raspberry Pi | Configuración de `spidev`; validación de CRC y descarte de tramas corruptas; escritura atómica del archivo de ventana; servidor HTTP y JSON; período de refresco |
| Integración | Refresco de extremo a extremo; tasa SPI experimental frente a la teórica; cambio de preset desde el dashboard; preset de bonificación de 41 667 Sa/s; control por canal |

---

## 2. Matriz de verificación

Los valores teóricos provienen de la estimación del Documento 1. Las pruebas **Parcial** tienen una parte ejecutada (sección 5) y otra pendiente.

| ID | Componente | Descripción | $f_s$ (Sa/s) | Criterio de aceptación | Resultado teórico | Estado |
|---|---|---|---|---|---|---|
| UT-MCU-01 | ESP32-S3 | Duración de la ventana y $f_s$ efectiva por preset | 100; 1 000; 10 000; 41 667 | $f_s$ real dentro de ±1 % (con señal de frecuencia conocida) | $T_w = 64/f_s$ | Parcial |
| UT-MCU-02 | ESP32-S3 | Conversión del ADC en CH1 y CH2 y orden de canales | 1 000 | Ganancia y offset dentro de la tolerancia del ADC; CH1 y CH2 no intercambiados | Ajuste lineal 0 a 4095 | Pendiente |
| UT-MCU-03 | ESP32-S3 | Llenado de 64 muestras por canal | todos | 64 muestras por canal por ventana, sin errores del ADC | 64 muestras | **Aprobada** |
| UT-MCU-04 | ESP32-S3 | Anillo de 3 slots bajo lectura SPI | 41 667 | Ninguna ventana alterada durante su entrega | Cero ventanas corruptas | **Aprobada** |
| UT-MCU-05 | ESP32-S3 | Segmento SPI (cabecera, datos y CRC) | todos | CRC válido en 100 % de los segmentos | 64 bytes por segmento | **Aprobada** |
| UT-MCU-06 | ESP32-S3 | Ejecución de comandos de preset | 100 a 41 667 | Cambio de frecuencia en ≤ 1 ventana | Cambio en ≤ 1 $T_w$ | **Aprobada** |
| UT-SBC-01 | Raspberry Pi | Configuración de `spidev` | — | Modo 0, velocidad y 8 bits confirmados | Parámetros aplicados | **Aprobada** |
| UT-SBC-02 | Raspberry Pi | Validación de CRC y descarte | 10 000 | Tramas corruptas descartadas y reintentadas | 0 tramas corruptas aceptadas | **Aprobada** |
| UT-SBC-03 | Raspberry Pi | Escritura atómica del archivo de ventana | — | Lectura nunca parcial desde el servidor | Archivo consistente | **Aprobada** |
| UT-SBC-04 | Servidor HTTP | Entrega del JSON de la última ventana | — | HTTP 200 y JSON válido | Respuesta en ≤ 100 ms | **Aprobada** |
| UT-SBC-05 | Servicio systemd | Reinicio automático del servicio | — | Servicio activo tras fallo simulado | Reinicio en ≤ 5 s | **Aprobada** |
| IT-01 | Sistema | Refresco del dashboard a 0,25 Hz | 1 000 | Actualización cada 4 s ± 0,2 s | $T_r = 4$ s | Parcial |
| IT-02 | Sistema | Refresco del dashboard a 1 Hz | 1 000 | Actualización cada 1 s ± 0,1 s | $T_r = 1$ s | Parcial |
| IT-03 | Enlace SPI | Transferencia de ventana a 100 Sa/s | 100 | $t_{tx}$ medido ≈ teórico y 0 errores de CRC | $D = 0{,}02$ % (20 MHz) | Parcial |
| IT-04 | Enlace SPI | Transferencia de ventana a 1 000 Sa/s | 1 000 | $t_{tx}$ medido ≈ teórico y 0 errores de CRC | $D = 0{,}20$ % (20 MHz) | Parcial |
| IT-05 | Enlace SPI | Transferencia de ventana a 10 000 Sa/s | 10 000 | $t_{tx}$ medido ≈ teórico y 0 errores de CRC | $D = 2{,}0$ % (20 MHz) | Parcial |
| IT-06 | Enlace SPI | Transferencia de ventana a 41 667 Sa/s | 41 667 | $t_{tx}$ medido ≈ teórico y $D < 15$ % | $D = 8{,}3$ % (20 MHz) | Parcial |
| IT-07 | Sistema | Preset de bonificación de extremo a extremo | 41 667 | Dashboard con la señal durante 5 min sin errores de CRC; huecos de WSEQ medidos | $f_s \geq 40\,000$ | **Aprobada** |
| IT-08 | Sistema | Cambio de preset desde el dashboard | 100 a 41 667 | Nuevo preset en ≤ 2 ventanas + 1 refresco | Cambio en ≤ 2 $T_w$ | Parcial |
| IT-09 | Sistema | Control por canal: encendido/apagado, presets distintos y 83 333 Sa/s | 100 a 83 333 | Cada cambio se refleja con el modo correcto, sin comandos pendientes | Modos sincronizado / independiente / un canal | **Aprobada** |

**Resumen:** 11 aprobadas, 8 parciales y 1 pendiente. A las parciales les falta: el analizador lógico (IT-03 a IT-06), una señal de frecuencia conocida (UT-MCU-01), medir las solicitudes del navegador (IT-01 e IT-02) y pulsar los botones en un navegador (IT-08). La pendiente es UT-MCU-02, la calibración del ADC.

---

## 3. Casos de prueba unitarios

### 3.1 ESP32-S3: adquisición y comunicación SPI

| ID | Procedimiento | Resultado esperado |
|---|---|---|
| UT-MCU-01 | Seleccionar cada preset (teclas 0 a 4 del monitor serie) y leer la duración de la adquisición (`t_adq`). Después, aplicar una señal periódica conocida y calcular $f_s$ a partir de su período en la ventana. | Duración compatible con $64/f_s$ más el arranque fijo de la ráfaga; $f_s$ real dentro de ±1 %. |
| UT-MCU-02 | Conectar cada canal a GND, a 3,3 V y a un divisor resistivo; registrar los códigos. Aplicar una tensión distinta en cada canal para comprobar el orden CH1/CH2. | Códigos acordes con el ajuste lineal; el canal de cada GPIO corresponde al índice esperado. |
| UT-MCU-03 | Contar los valores por canal en cada línea `WIN` del monitor serie. | Exactamente 64 muestras por canal en cada ventana y `err_adc = 0`. |
| UT-MCU-04 | Con el preset de 41 667 Sa/s, leer ventanas por SPI durante varios minutos y comprobar que cada ventana recibida es consistente (misma WSEQ en los 5 segmentos). | Ninguna ventana mezclada o alterada. |
| UT-MCU-05 | Comparar el CRC calculado en el chip con el vector estándar y validar los segmentos recibidos en la Raspberry Pi. | CRC `0x29B1` para el vector de prueba; 100 % de segmentos válidos. |
| UT-MCU-06 | Enviar SET_PRESET por SPI y medir en cuántas ventanas cambia la frecuencia del encabezado. | Cambio en la ventana siguiente y sin reinicio del firmware. |

### 3.2 Raspberry Pi: servicio SPI y servidor HTTP

| ID | Procedimiento | Resultado esperado |
|---|---|---|
| UT-SBC-01 | Ejecutar `oscd --bench` y leer la línea `spidev` que imprime el servicio. | Modo 0, 8 bits y velocidad máxima igual a la solicitada. |
| UT-SBC-02 | Ejecutar `make test` y, en la Raspberry Pi, revisar los contadores de errores durante el uso del bus real. | Tramas corruptas descartadas y contadas; ninguna publicada. |
| UT-SBC-03 | Leer `window.json` en un bucle mientras el servicio lo reescribe, validando cada lectura como JSON. | Ninguna lectura parcial. |
| UT-SBC-04 | Solicitar `/data/window.json` repetidamente durante un minuto. | HTTP 200, JSON válido y respuesta en ≤ 100 ms. |
| UT-SBC-05 | Terminar el proceso con `kill -9`. | Reinicio automático en ≤ 5 s. |

---

## 4. Casos de prueba de integración

### 4.1 Refresco del dashboard (0,25 Hz y 1 Hz)

1. Configurar la tasa de refresco en 0,25 Hz desde el dashboard (o con `curl -X POST 'http://<ip>/api/refresh?hz=0.25'`).
2. Registrar con las herramientas de desarrollo del navegador las marcas de tiempo de cada solicitud a `window.json` durante 5 minutos.
3. Calcular el intervalo entre solicitudes y comparar con $T_r = 4$ s.
4. Repetir con 1 Hz ($T_r = 1$ s).
5. Criterio de aceptación: intervalo dentro de ±0,2 s (0,25 Hz) y ±0,1 s (1 Hz).

### 4.2 Verificación experimental de la tasa SPI

1. Fijar la velocidad solicitada en la Raspberry Pi y registrar la frecuencia de reloj real con el analizador lógico.
2. Para cada preset, ejecutar `oscd --bench 1000` y capturar SCLK y CS.
3. Medir el tiempo entre el flanco de bajada y el de subida de CS en cada segmento, y el tiempo entre segmentos.
4. Calcular el promedio y la desviación estándar de $t_{tx}$ (solo bits de reloj: $5 \times 512 / f_{SPI}$).
5. Comparar contra $t_{tx}^{teo} = 2\,560/f_{SPI}$ y calcular el error relativo:

$$\varepsilon = \frac{\bar{t}_{tx}^{\,exp} - t_{tx}^{\,teo}}{t_{tx}^{\,teo}} \cdot 100\ \%$$

Con `oscd --bench` se registran además el tiempo medio y máximo del `ioctl` por transacción, las ventanas por segundo y los contadores de ventanas no leídas, duplicadas y errores de CRC.

### 4.3 Protocolo de verificación a 41 667 Sa/s (bonificación)

1. Configurar el preset 3 mediante comando SPI (dashboard u `oscd --preset 3`).
2. Comprobar con una señal de frecuencia conocida que la $f_s$ real es 41 667 Sa/s ± 1 % ($T_w = 1{,}536$ ms más el arranque de la ráfaga de ≈ 75 µs).
3. Registrar 5 minutos de adquisición con el dashboard activo a 1 Hz.
4. Registrar los contadores del servicio: ventanas recibidas, no leídas (saltos de WSEQ), errores de CRC y de sincronía. La captura es por refresco de ventanas, de modo que la Raspberry Pi no necesita leer todas las ventanas: el criterio es **cero ventanas corruptas** y que la ventana mostrada tenga la $f_s$ correcta.
5. Verificar que el ciclo de trabajo de los bits es $D < 15$ %.
6. Documentar cualquier desviación y su causa probable.

---

## 5. Registro de resultados

### 5.1 UT-MCU-01: duración de la ventana por preset (2 de noviembre de 2026)

Medido con el contador de ciclos del ESP32-S3 alrededor de la adquisición de la ventana. Evidencia: [`UT-MCU-01_serial_presets.log`](Resultados_Pruebas/UT-MCU-01_serial_presets.log).

| Preset ($f_s$) | $T_w$ teórico | $T_w$ medido | Diferencia | Observación |
|---|---|---|---|---|
| 100 Sa/s | 640,000 ms | 640,093 ms | +0,09 ms (+0,01 %) | Temporizador absoluto |
| 1 000 Sa/s | 64,000 ms | 64,318 ms | +0,32 ms (+0,50 %) | Ráfaga DMA |
| 10 000 Sa/s | 6,400 ms | 6,492 ms | +0,09 ms (+1,4 %) | Ráfaga DMA |
| 41 667 Sa/s | 1,536 ms | 1,609 ms | +0,07 ms (+4,8 %) | Ráfaga DMA |

*La diferencia en los presets DMA es compatible con un tiempo fijo de arranque de la ráfaga de 75 a 90 µs.*

**Parcial:** la ráfaga completa dura lo esperado, pero la $f_s$ efectiva debe confirmarse con una señal de frecuencia conocida. Con una onda cuadrada de 500 Hz en la entrada, el dashboard midió 499,5–500,0 Hz a 10 kSa/s (sección 5.6), lo que es coherente con la $f_s$ nominal, aunque la señal no era de frecuencia certificada.

### 5.2 Pruebas iniciales (2 de noviembre de 2026)

- **UT-MCU-03:** cada línea `WIN` del monitor serie contiene 64 valores por canal y `err_adc` permaneció en 0 en todas las capturas.
- **UT-MCU-05:** el CRC-16 del vector `"123456789"` calculado en el chip es `0x29B1` ([`UT-MCU-05_crc_autotest.log`](Resultados_Pruebas/UT-MCU-05_crc_autotest.log)). En la integración, 0 errores de CRC en cientos de miles de segmentos (sección 5.4).
- **UT-SBC-02:** el protocolo de la Raspberry Pi pasa las pruebas unitarias en el host, entre ellas el descarte de una trama con un bit alterado y una con sincronía errónea ([`UT-SBC-02_test_protocol_host.log`](Resultados_Pruebas/UT-SBC-02_test_protocol_host.log)). Hoy son 16 pruebas, también correctas en la propia Raspberry Pi.

### 5.3 Raspberry Pi 4 sin el ESP32-S3 (2 de noviembre de 2026)

Raspberry Pi 4 Model B Rev 1.2, kernel 6.18. Las pruebas de servidor, API y servicio usan `oscd --demo` (ventanas sintéticas con el mismo formato JSON); no prueban el enlace SPI. Evidencia: [`UT-SBC-01_spidev_ioctl.log`](Resultados_Pruebas/UT-SBC-01_spidev_ioctl.log) y [`UT-SBC-03-05_IT-01-02_raspberry.log`](Resultados_Pruebas/UT-SBC-03-05_IT-01-02_raspberry.log).

| ID | Resultado medido | Conclusión |
|---|---|---|
| UT-SBC-01 | `spidev0.0` acepta modo 0, 8 bits y las velocidades pedidas (1; 8; 15,6; 20 y 31,25 MHz) | Aprobada; el reloj efectivo queda por medir con analizador lógico |
| UT-SBC-03 | 189 609 lecturas directas de `window.json` mientras el servicio lo reescribía: 0 parciales, 25 versiones distintas | Aprobada: la escritura con `rename` es atómica |
| UT-SBC-04 | 300 solicitudes HTTP (una cada 100 ms): 300 con código 200 y JSON válido; máximo 48,7 ms | Aprobada (≤ 100 ms) |
| UT-SBC-05 | `kill -9` al servicio: nuevo PID y HTTP 200 en 1,09 s | Aprobada (≤ 5 s) |
| IT-01 | Escritura del JSON a 0,25 Hz: 8 intervalos, media 4,000 s, rango 4,000 a 4,001 s | Parcial: falta medir las solicitudes del navegador |
| IT-02 | Escritura del JSON a 1 Hz: 11 intervalos, media 1,001 s, rango 1,000 a 1,001 s | Parcial: falta medir las solicitudes del navegador |

Latencia del `ioctl` de una transacción de 64 bytes (sin ESP32-S3, MISO flotante):

| Velocidad solicitada | Tiempo ideal de bits | `ioctl` medio | `ioctl` máximo |
|---|---|---|---|
| 1 MHz | 512,0 µs | 590,5 µs | 625,3 µs |
| 8 MHz | 64,0 µs | 96,5 µs | 146,9 µs |
| 15,625 MHz | 32,8 µs | 58,7 µs | 101,7 µs |
| 20 MHz | 25,6 µs | 52,6 µs | 111,8 µs |
| 31,25 MHz | 16,4 µs | 29,5 µs | 104,2 µs |

El `ioctl` añade entre 13 y 78 µs sobre el tiempo ideal de bits.

### 5.4 Integración ESP32-S3 + Raspberry Pi 4 (5 de noviembre de 2026)

Montaje: ESP32-S3-DevKitC-1 (WROOM-1 N8R2) cableado a la Raspberry Pi 4 según el Documento 1, cables cortos y GND común. Medición con `oscd --bench`. Evidencia: [`IT-03-08_integracion_spi.log`](Resultados_Pruebas/IT-03-08_integracion_spi.log).

**Velocidad del reloj SPI** (preset de 41 667 Sa/s, guarda de 150 µs, 3 000 ventanas u 8 s):

| Reloj solicitado | Ventanas/s | `ioctl` medio | No leídas | Errores CRC | Errores sync |
|---|---|---|---|---|---|
| 1 MHz | 184,7 | 809,6 µs | 3 010 | 0 | 337 |
| 8 MHz | 579,8 | 165,1 µs | 112 | 0 | 553 |
| 15,625 MHz | 594,2 | 91,8 µs | 37 | 0 | 612 |
| 20 MHz | 585,9 | 79,0 µs | 6 | 0 | 883 |

**Tiempo de guarda** (20 MHz, 41 667 Sa/s): con 50 µs el esclavo no alcanza a rearmarse y se pierden muchas ventanas; con 100 y 150 µs se reciben 587 a 590 ventanas/s con menos de 1,1 % no leídas; con 250 µs baja a 554 ventanas/s y con 400 µs a 382. Se adoptó **20 MHz y 150 µs** como configuración del servicio. Estas corridas se hicieron antes de corregir los defectos de la sección 5.5, así que algunos conteos de ventanas no leídas (en especial el de 50 µs) pueden estar inflados; los errores de CRC no se ven afectados.

**Presets** (20 MHz, 150 µs; el cambio se ordena por SPI al inicio de cada corrida):

| Preset | Ventanas/s recibidas | Ventanas/s esperadas | Tiempo hasta ver el preset nuevo | Errores CRC |
|---|---|---|---|---|
| 100 Sa/s | 1,6 | 1,56 | 649 ms | 0 |
| 1 000 Sa/s | 15,0 | 15,5 | 661 ms | 0 |
| 10 000 Sa/s | 146,1 | 154 | 29 ms | 0 |
| 41 667 Sa/s | 587,0 | 621 | 11 ms | 0 |

*Las ventanas/s esperadas son $1/T_w$ con la duración de la sección 5.1; entre ráfagas el firmware añade además el tiempo de publicar la ventana, por lo que a 41 667 Sa/s produce en la práctica ≈ 595 ventanas/s.* El tiempo de cambio incluye terminar la ventana en curso: al pasar de 100 Sa/s a 1 000 Sa/s debe terminar una ventana de 640 ms, de ahí los 661 ms. En todos los casos el cambio ocurre en la ventana siguiente (UT-MCU-06).

**IT-07: estabilidad de 5 minutos a 41 667 Sa/s**, con el servicio `oscd` (20 MHz, 150 µs) y lectura de `window.json` por Nginx cada segundo:

- Duración: 332 s; **194 668 ventanas** recibidas (586,3 ventanas/s).
- **Errores de CRC: 0**; ventanas hacia atrás: 0; errores de E/S: 0; reinicios del servicio: 0.
- Ventanas producidas que se leyeron: **99,6 %** (783 no leídas).
- Lecturas HTTP del JSON: 300 de 300 correctas, todas con preset 3 y 64 muestras por canal.
- Errores de sincronía: 54 274 (segmentos clocados antes de que el esclavo se rearmara); se descartan sin consecuencias y se recupera en el siguiente segmento 0.

**IT-03 a IT-06 (parciales).** El enlace transfiere ventanas en los cuatro presets con 0 errores de CRC. El `ioctl` medio a 20 MHz es ≈ 79 µs por segmento (≈ 395 µs por ventana), incluida la sobrecarga del kernel. Falta medir con analizador lógico la frecuencia real del reloj y $t_{tx}$ de los bits, para compararlos con el teórico $2\,560/f_{SPI} = 128$ µs.

**IT-08 (parcial).** El cambio de preset con la misma llamada que usan los botones del dashboard (`POST /api/preset`, enviada con `curl`) se reflejó en `window.json` en menos de 2 s, y el dashboard real renderizado mostró el preset elegido (capturas en [`Imagenes/`](../Imagenes/)). Falta que una persona pulse los botones en un navegador.

### 5.5 Defectos encontrados y corregidos durante la integración

| Defecto | Causa | Corrección |
|---|---|---|
| El preset no cambiaba al iniciar `oscd` | El número del comando se asignaba antes de conocer el `CMD_ACK` del esclavo; si coincidía con uno ya ejecutado, el ESP32-S3 lo ignoraba y la Raspberry Pi lo daba por confirmado | El número se asigna después de alinearse con `CMD_ACK` y nunca se envía antes |
| Salto falso de miles de ventanas «no leídas» al arrancar el servicio | Sin lecturas, el esclavo deja congelada la ventana de ese momento, que puede tener decenas de segundos | Se descarta la primera ventana tras arrancar; los saltos hacia atrás se cuentan aparte y no se publican |
| El servicio dejaba de publicar tras un reinicio del ESP32-S3 | El contador WSEQ vuelve a 0 y las ventanas nuevas parecían «hacia atrás» | Un salto hacia atrás de 16 o más ventanas se interpreta como reinicio: se acepta y se reenvían la máscara y los presets elegidos |
| CH1 en 0 constante a 100 Sa/s con la primera placa | **Hipótesis no confirmada:** se atribuyó a que la primera conversión de cada ráfaga DMA vale 0. Más tarde se comprobó que una señal periódica muestreada a un submúltiplo de su frecuencia también produce un valor constante (*aliasing*, sección 5.6) | Se descarta una muestra al inicio de cada ráfaga como precaución (no afecta la temporización) |

Pruebas de las correcciones: un reinicio del servicio tras 20 s sin lecturas dio 0 ventanas no leídas. Con el preset de 10 000 Sa/s se reinició el ESP32-S3 por su línea de *reset*: el servicio registró «ESP32 reiniciado (WSEQ 9028 → 0)», el ESP32-S3 volvió con el preset de 10 000 Sa/s y la publicación continuó sin intervención (un segmento con CRC inválido durante el reinicio, descartado).

### 5.6 IT-09: muestreo independiente por canal y formato v2 (5 de noviembre de 2026)

Firmware con preset y activación por canal, formato de ventana versión 2 y preset de 83 333 Sa/s; `oscd` con cola de comandos y compatibilidad v1/v2 (16 pruebas unitarias correctas en el host y en la Raspberry Pi). Evidencia: [`IT-09_muestreo_por_canal_v2.log`](Resultados_Pruebas/IT-09_muestreo_por_canal_v2.log).

| Configuración | Modo | Duración medida | Comentario |
|---|---|---|---|
| CH1 y CH2 a 10 kSa/s | sincronizado | 6,6 ms | 64 pares en una ráfaga |
| Solo CH1 a 83 333 Sa/s | un canal | 0,854 ms | ≈ 1 130 ventanas/s |
| CH1 y CH2 a 83 333 Sa/s | independiente | 1,73 ms | dos ráfagas seguidas |
| CH1 a 83 333 Sa/s y CH2 a 100 Sa/s | independiente | 640,9 ms | domina la ventana lenta |

**Control por canal de extremo a extremo.** Por la API publicada por Nginx se ejecutó la secuencia: ambos a 1 kSa/s (sincronizados), CH2 a 10 kSa/s (independientes), apagar CH2, CH1 a 83 333 Sa/s, encender CH2 y volver a 1 kSa/s. Cada cambio se reflejó en `window.json` con el modo correcto, sin comandos pendientes ni vencidos. Con el firmware anterior (v1), un comando por canal venció a las 400 transacciones sin bloquear el siguiente comando.

**Señal de prueba en las entradas.** Durante estas pruebas las dos entradas tenían una onda cuadrada de 500 Hz entre 0 V y 1,10 V, con 50 % de ciclo útil y en fase. El dashboard midió 499,5–500,0 Hz a 10 kSa/s (20 muestras por período, aviso verde). A 1 kSa/s midió 500 Hz con solo 2 muestras por período y mostró el aviso de *aliasing*, recomendando 10 kSa/s. A 100 Sa/s la señal aparece constante: 500 Hz es múltiplo de la frecuencia de muestreo y siempre se muestrea la misma fase. El diagnóstico con la tecla `d` confirmó que todas las conversiones DMA eran válidas y que los valores nulos eran el nivel bajo de la señal.

### 5.7 Pendiente sobre las entradas

UT-MCU-02 sigue pendiente: conectar GND, 3,3 V y un divisor a cada canal, con una tensión distinta en cada uno, para fijar ganancia y offset y confirmar el orden CH1/CH2. Con las entradas sin conectar, ambos canales muestran ruido de 0 a 4095 (entradas flotantes que captan la red eléctrica); es el comportamiento esperado.
