# Documento 5 — Declaración de uso de inteligencia artificial

**Sistema Híbrido de Adquisición Analógica mediante SPI y Dashboard Embebido en Linux**
Universidad Nacional de Colombia — Sede Manizales · Programación de sistemas Linux embebido · Noviembre de 2026

## 1. Modelo y herramienta

| Campo | Valor |
|---|---|
| Asistente | Claude (Anthropic), modelos **Claude Sonnet 5.5** (`claude-sonnet-5-5`) y, desde el 5 de noviembre de 2026, **Claude Opus 5.5** (`claude-opus-5-5`) |
| Herramienta | **Claude Code** (CLI), con acceso a archivos, PowerShell, WSL (Ubuntu), SSH a la Raspberry Pi y lectura de páginas web |
| Periodo | Desde el 1 de noviembre de 2026 (una sesión continua) |
| Usuario | Integrante responsable del repositorio |

## 2. Tareas en las que se usó la IA

| Tarea | Qué hizo la IA | Qué hicieron las personas |
|---|---|---|
| Revisión de la documentación inicial | Leyó los dos documentos iniciales y el enunciado y listó la información faltante | Respondieron y fijaron las decisiones (placa, pines, Zephyr, repositorio, roles) |
| Protocolo SPI | Propuso el formato de segmentos de 64 bytes tras comprobar en el código de Zephyr que el esclavo del ESP32-S3 usa FIFO (64 B por CS); luego la versión 2 con control por canal | Aprobar y ajustar |
| Firmware del ESP32-S3 | Escribió el código (ADC con DMA, esclavo SPI, monitor serie, muestreo por canal), el overlay y la configuración; compiló y cargó la placa | Proveer el entorno y el hardware; cablear; revisar |
| Software de la Raspberry Pi | Escribió el servicio en C, el dashboard, la configuración de Nginx y systemd; lo compiló, instaló y probó por SSH | Proveer acceso; revisar; conectar el ESP32-S3 |
| Documentación | Redactó y reorganizó los documentos 1 a 6 (primero en LaTeX y luego en Markdown) y los README | Revisar; aportar nombres, cédulas, roles y el video |
| Pruebas | Diseñó la estrategia y ejecutó las pruebas de software, de integración y de estabilidad en el hardware real | Ejecutar las pruebas que requieren instrumentos (analizador lógico, señales conocidas) |
| Repositorio | Creó la estructura de carpetas, el `.gitignore` y los commits | Autenticar y publicar en GitHub |

## 3. Cómo se verificó el resultado

| Elemento | Verificación realizada | Alcance |
|---|---|---|
| Firmware ESP32-S3 | Compilación sin errores ni advertencias; carga con verificación de hash; lectura del monitor serie en la placa real | Verificado en hardware (5 presets, muestreo por canal) |
| Tiempos de ventana | Medidos en el chip con el contador de ciclos y comparados con la teoría | Parcial: falta confirmar $f_s$ con una señal certificada |
| CRC-16 | Vector `"123456789"` → `0x29B1` en el ESP32-S3 y en la Raspberry Pi | Verificado |
| Protocolo del lado Raspberry Pi | 16 comprobaciones automáticas en el host y en la propia Raspberry Pi | Verificado |
| `spidev`, servicio, Nginx, API, systemd | Ejecutados en la Raspberry Pi 4; mediciones de HTTP, escritura atómica, cadencia de refresco y reinicio | Verificado |
| Enlace SPI con el ESP32-S3 | Banco de pruebas en el hardware cableado: 4 velocidades, 5 tiempos de guarda, presets y estabilidad de 5 minutos a 41 667 Sa/s con 0 errores de CRC. La IA encontró y corrigió defectos de su propio código ([Documento 2](02_Matriz_verificacion_y_plan_de_pruebas.md), sección 5.5) | Verificado; falta el analizador lógico |
| Muestreo por canal | Comandos por canal de extremo a extremo y duraciones medidas | Verificado en hardware |
| Dashboard | Renderizado con Microsoft Edge sin interfaz gráfica contra datos sintéticos (incluidas tensiones negativas) y contra la Raspberry Pi real; capturas en `Imagenes/` | Verificado el renderizado y las mediciones; falta que una persona use los controles |
| Consola UART de la Raspberry Pi | Texto transmitido en ambos sentidos y *login* obtenido desde el PC | Verificado |
| Diagrama de pines de la placa | La IA interpretó una imagen aportada por el usuario; en una respuesta explicó mal el origen de unas etiquetas del diagrama y lo corrigió después ([Documento 6](06_Registro_conversaciones.md)). Las funciones FSPI usadas coinciden con el código de Zephyr | Verificado con el enlace funcionando |
| Cálculos de la estimación SPI | Revisados a mano por la IA | Pendiente de revisión humana |

Las afirmaciones sobre el hardware (límites del ADC, FIFO de 64 bytes del esclavo) se tomaron del código fuente de Zephyr y de sus HAL instalados en el equipo, o se marcan como *por verificar* en los documentos. En dos ocasiones la IA formuló hipótesis equivocadas y las corrigió con datos: el origen de las etiquetas del diagrama de pines y la causa de los valores nulos del ADC (que resultaron ser el nivel bajo de una señal real). Ambas están registradas en el Documento 6.

## 4. Contenido de los chats

El registro de las interacciones está en el [Documento 6](06_Registro_conversaciones.md). La transcripción completa generada por la herramienta se exporta con el script [`IA/exportar_chats.ps1`](IA/exportar_chats.ps1) a la carpeta `IA/Transcripciones_crudas/`, excluida de Git porque contiene rutas locales y metadatos que deben revisarse antes de publicarse. Se omitieron del registro las credenciales que el usuario compartió en el chat.

## 5. Responsabilidad

Los integrantes revisaron y asumen la responsabilidad del contenido entregado. Las decisiones de diseño finales corresponden al equipo.
