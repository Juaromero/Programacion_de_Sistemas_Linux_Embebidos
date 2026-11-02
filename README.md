# Programación de Sistemas Linux Embebidos

Repositorio de la materia **Programación de sistemas Linux embebido** (Ingeniería Electrónica, Universidad Nacional de Colombia, sede Manizales), donde se recopilan los proyectos desarrollados durante el semestre.

## Plataforma

| Componente | Detalle |
|---|---|
| SBC | **Raspberry Pi 4 Model B** (Raspberry Pi OS, Linux) |
| Microcontrolador | **ESP32-S3-N16R8** (Xtensa LX7, 16 MB flash, 8 MB PSRAM) |
| RTOS del microcontrolador | **Zephyr 4.5** |
| Toolchain | Zephyr SDK 1.0.1 (`xtensa-espressif_esp32s3_zephyr-elf`), CMake + Ninja, `west` |
| Servidor web | **Nginx** |

## Estructura del repositorio

```
Programacion_de_Sistemas_Linux_Embebidos/
├── Proyecto_1/    (pendiente)
├── Proyecto_2/    Osciloscopio de 2 canales: ESP32-S3 + SPI + Raspberry Pi + dashboard
└── Proyecto_3/    (pendiente)
```

Cada proyecto contiene:

```
<Proyecto>/
├── Codigo/        Código fuente (firmware, software del SBC, configuración)
├── Documentos/    Documentación, resultados de pruebas y declaración de uso de IA
├── Imagenes/      Diagramas, capturas y fotografías
└── README.md
```

## Proyectos

| Proyecto | Descripción | Estado |
|---|---|---|
| [Proyecto_1](Proyecto_1/) | Por definir | Pendiente |
| [Proyecto_2](Proyecto_2/) | **Sistema híbrido de adquisición analógica por SPI y dashboard embebido** (osciloscopio) | En desarrollo |
| [Proyecto_3](Proyecto_3/) | Por definir | Pendiente |
