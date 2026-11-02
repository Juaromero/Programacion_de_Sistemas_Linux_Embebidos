# Proyecto 2 — Sistema híbrido de adquisición analógica por SPI y dashboard embebido (Osciloscopio)

Osciloscopio de **2 canales**: un **ESP32-S3-N16R8** (Zephyr) digitaliza dos señales con su ADC de 12 bits y las entrega por **SPI** (esclavo) a una **Raspberry Pi 4 Model B** (maestro), que las publica en un **dashboard web** servido por **Nginx**. Las ventanas también se ven en el monitor serie del ESP32-S3.

* Ventanas de **64 muestras por canal**, frecuencia de muestreo constante dentro de cada ventana.
* Presets de muestreo intercambiables por SPI: **100, 1 000, 10 000 y 41 667 Sa/s** (este último cumple la bonificación ≥ 40 kSa/s).
* Captura por **refresco de ventanas**: el ESP32-S3 adquiere de forma continua; la Raspberry Pi toma la más reciente.
* Refresco del dashboard configurable entre **0,25 Hz** (4 s) y **1 Hz**.

## Estructura

```
Proyecto_2/
├── Codigo/
│   ├── Firmware_ESP32S3/     Aplicación Zephyr (ADC + DMA, SPI esclavo, monitor serie)
│   │   ├── CMakeLists.txt, prj.conf
│   │   ├── boards/           Overlay de pines/ADC/SPI (+ usb_console.overlay opcional)
│   │   └── src/              main, acq, spi_link, protocol
│   └── Software_RPi/         Servicio oscd (C), dashboard, nginx y systemd
│       ├── src/              oscd.c, spi_master.c, protocol.c
│       ├── web/              index.html + uPlot (local, sin CDN)
│       ├── config/           oscd.service, nginx-osciloscopio.conf
│       ├── tests/            test_protocol.c (sin hardware)
│       ├── Makefile, install.sh
├── Documentos/
│   ├── Latex/                  6 documentos LaTeX + preámbulo y portada compartidos + Makefile
│   ├── PDF/                    PDF generados (make en Latex/)
│   ├── IA/                     exportar_chats.ps1 (transcripciones crudas, fuera de Git)
│   └── Resultados_Pruebas/     Evidencias (logs, capturas)
└── Imagenes/
```

## Conexiones

| Señal | ESP32-S3 | Raspberry Pi 4 (pin físico) |
|---|---|---|
| SCLK | GPIO12 | GPIO11 / SCLK (23) |
| MOSI | GPIO11 | GPIO10 / MOSI (19) |
| MISO | GPIO13 | GPIO9 / MISO (21) |
| CS | GPIO10 | GPIO8 / CE0 (24) |
| GND | GND | GND (25) |
| **CH1** (entrada) | **GPIO1** (ADC1_CH0) | — |
| **CH2** (entrada) | **GPIO2** (ADC1_CH1) | — |

Entradas analógicas: **0 a 3,1 V** (atenuación 12 dB). No superar 3,3 V. Cables SPI cortos y GND común.

## 1. Firmware del ESP32-S3 (WSL + Zephyr)

Requisitos: Zephyr 4.5 con su SDK 1.0.1 y `west` (entorno virtual del workspace).

```bash
source ~/zephyrproject/.venv/bin/activate
cd ~/zephyrproject
west build -p auto -b esp32s3_devkitc/esp32s3/procpu \
    /ruta/al/repo/Proyecto_2/Codigo/Firmware_ESP32S3 -d ~/build/osc
west flash -d ~/build/osc --esp-device /dev/ttyACM1
```

* La consola es **UART0** (puerto «UART» de la DevKitC, puente CH343) a **115200 8N1**. Para usar el puerto USB nativo: añadir `-- -DEXTRA_DTC_OVERLAY_FILE=boards/usb_console.overlay`.
* **WSL y USB:** el CH343 debe estar *adjunto* a WSL desde Windows: `usbipd list` y `usbipd attach --wsl --busid <BUSID>` (el nodo puede ser `/dev/ttyACM0` o `ACM1`; un nodo huérfano de una sesión anterior hace que `esptool` se cuelgue).
* **Monitor serie:** `stty -F /dev/ttyACM1 115200 raw -echo; cat /dev/ttyACM1`. Cada segundo imprime una línea `STAT` y las líneas `WIN … CH1/CH2` con la última ventana. Las teclas **0, 1, 2, 3** cambian el preset sin necesidad de la Raspberry Pi.

## 2. Software de la Raspberry Pi 4

Requisitos: Raspberry Pi OS, SSH, SPI0 habilitado.

```bash
cd Proyecto_2/Codigo/Software_RPi
sudo ./install.sh            # paquetes, SPI, nginx, compilación, pruebas, systemd
sudo reboot                  # si SPI/core_freq acaban de habilitarse
sudo systemctl start oscd
```

Abrir `http://<ip-de-la-raspberry>/`. Comandos útiles:

```bash
make test                                        # pruebas unitarias del protocolo (sin hardware)
sudo ./oscd --bench 1000 --speed 8000000 --gap-us 150     # banco de pruebas SPI
sudo ./oscd --bench 1000 --preset 3 --speed 15625000       # preset de bonificación
curl -X POST 'http://localhost/api/preset?id=2'            # cambiar preset
curl -X POST 'http://localhost/api/refresh?hz=0.25'        # cambiar refresco
curl http://localhost/data/window.json                     # última ventana
journalctl -u oscd -f                                      # registro del servicio
```

Parámetros de `oscd`: `--dev`, `--speed` (Hz), `--gap-us` (guarda entre transacciones), `--out`, `--port`, `--refresh` (0,25 a 1), `--preset`, `--bench N`, `--bench-secs S`, `--demo`, `--rt`, `-v`.

## 3. Puesta en marcha recomendada

1. Firmware solo, con monitor serie (tecla 0–3). Comprobar `err_adc=0` y `crc_test=0x29b1`.
2. Calibrar el ADC con GND, 3,3 V y un divisor (ver Documento 2).
3. Conectar el SPI; arrancar con `--speed 1000000` y subir hasta la velocidad estable; ajustar `--gap-us`.
4. Editar `config/oscd.service` con los valores estables y arrancar el servicio.

## Probar la Raspberry Pi sin el ESP32-S3 (modo demo)

```bash
sudo ./oscd --demo --out /run/osciloscopio/window.json --port 8081   # ventanas sintéticas
```

Genera ventanas con el mismo formato (senoides) y responde a `/api/preset` y `/api/refresh`; sirve para probar Nginx, el dashboard y el servicio sin el enlace SPI.

## Estado

| Elemento | Estado |
|---|---|
| Firmware compila y se ejecuta en el ESP32-S3 (4 presets, ADC con DMA, monitor serie) | Verificado en hardware |
| Duración de ventana por preset (incl. 41 667 Sa/s) | Medido (Documento 2) |
| CRC-16 compatible ESP32-S3 ↔ Raspberry Pi | Verificado (`0x29B1`) |
| `oscd` compila y pasa `make test` en la Raspberry Pi 4 | Verificado |
| `spidev0.0` (modo 0, 8 bits) y latencia del `ioctl` | Medido |
| Nginx + API + JSON atómico + refresco + reinicio systemd (modo demo) | Verificado en la Raspberry Pi |
| Dashboard renderizado en un navegador | **Pendiente** |
| Enlace SPI ESP32-S3 ↔ Raspberry Pi y pruebas de integración | **Pendiente** (placas sin cablear) |
| Los `.tex` | Reorganizados; revisión estática OK, **sin compilar** (sin LaTeX en el equipo) |

## Documentación

Todos los documentos están en LaTeX en [`Documentos/Latex`](Documentos/Latex/) (ver su README para compilar):

1. Especificación técnica y arquitectura
2. Matriz de verificación y plan de pruebas
3. Protocolo SPI
4. Declaración de participación
5. Declaración de uso de IA
6. Registro de conversaciones
