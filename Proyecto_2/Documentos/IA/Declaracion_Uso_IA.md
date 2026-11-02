# Declaración de uso de inteligencia artificial

Proyecto 2 — Sistema híbrido de adquisición analógica por SPI y dashboard embebido.

## 1. Modelo y herramienta

| Campo | Valor |
|---|---|
| Asistente | Claude (Anthropic), modelo **Claude Sonnet 5.5** (`claude-sonnet-5-5`) |
| Herramienta | **Claude Code** (CLI) con acceso a archivos, PowerShell, WSL (Ubuntu) y navegación web |
| Periodo | 1 – 2 de noviembre de 2026 (primera sesión) |
| Usuario | Integrante responsable del repositorio |

## 2. Tareas en las que se usó la IA

| Tarea | Qué hizo la IA | Qué hicieron las personas |
|---|---|---|
| Revisión de la documentación inicial | Leyó los dos `.tex` y el enunciado y listó la información faltante | Respondió las preguntas y fijó las decisiones (placa, pines, Zephyr, repositorio) |
| Diseño del protocolo SPI | Propuso el formato de segmentos de 64 bytes tras comprobar en el código fuente de Zephyr que el esclavo del ESP32-S3 usa FIFO (límite de 64 B por CS) | Aprobar/ajustar |
| Firmware del ESP32-S3 (Zephyr) | Escribió el código (adquisición ADC+DMA, esclavo SPI, monitor serie), el overlay y `prj.conf`; compiló y cargó la placa | Proveer entorno y hardware; revisar |
| Software de la Raspberry Pi | Escribió el servicio `oscd` en C, el dashboard (uPlot), la configuración de Nginx/systemd y el instalador | Instalar y ejecutar en la Raspberry Pi (pendiente) |
| Documentación | Reescribió los documentos 1 y 2 (LaTeX), el protocolo, los README y la estructura del repositorio | Revisar, completar nombres/cédulas, compilar |
| Pruebas | Diseñó la estrategia, ejecutó las pruebas que no requieren la Raspberry Pi y registró evidencias | Ejecutar las pruebas con instrumentos, la Raspberry Pi y señales reales |
| Repositorio | Creó la estructura de carpetas, `.gitignore` y los commits | Autorizar y verificar lo publicado |

## 3. Cómo se verificó el resultado

| Elemento | Verificación realizada | Alcance |
|---|---|---|
| Firmware ESP32-S3 | Compilación con `west build` sin errores; carga con `west flash` y hash verificado; lectura del monitor serie en la placa real | Ejecuta, adquiere y mide tiempos en los 4 presets |
| Tiempos de ventana por preset | Medidos en el chip con el contador de ciclos (UT-MCU-01) y comparados con la teoría | Parcial: falta confirmar $f_s$ con señal de frecuencia conocida |
| CRC-16 | Vector estándar `"123456789"` → `0x29B1` en el ESP32-S3 y en el código de la Raspberry Pi (host) | Verificado |
| Protocolo del lado Raspberry Pi | `make test`: 11 comprobaciones en el host (WSL) | Verificado en host |
| JSON de `oscd` | El JSON generado por la misma función `build_json()` se validó con un parser JSON | Verificado en host |
| Servicio `oscd`, Nginx, systemd, dashboard | **No verificado**: no hubo acceso a la Raspberry Pi y no se renderizó el dashboard en un navegador | Pendiente |
| Enlace SPI extremo a extremo | **No verificado**: pendiente de cableado y pruebas | Pendiente |
| Documentos `.tex` | **No compilados**: no hay LaTeX instalado en el equipo de trabajo | Pendiente |
| Cálculos de la estimación SPI | Revisados a mano por la IA (por ejemplo, umbral de 11,1 MHz para RNF-02); deben ser revisados por el equipo | Pendiente de revisión humana |

Las afirmaciones sobre capacidades del hardware (límites del ADC, FIFO de 64 bytes del esclavo, divisores de SPI0) se tomaron del código fuente de Zephyr y de sus HAL instalados en el equipo, o se marcan como *por verificar* en los documentos. Los divisores del SPI0 de la Raspberry Pi 4 y el reloj real alcanzable deben confirmarse con el analizador lógico.

## 4. Contenido de los chats

El registro de las interacciones está en [Registro_Conversaciones.md](Registro_Conversaciones.md). Las transcripciones completas generadas por la herramienta se exportan con [exportar_chats.ps1](exportar_chats.ps1) a `Transcripciones_crudas/` (carpeta excluida de Git por contener rutas locales y metadatos que deben revisarse antes de publicarse).

## 5. Responsabilidad

Los integrantes revisaron y asumen la responsabilidad del contenido entregado. Las decisiones de diseño finales corresponden al equipo.
