# Documento 4 — Declaración de participación y aportes

**Sistema Híbrido de Adquisición Analógica mediante SPI y Dashboard Embebido en Linux**
Universidad Nacional de Colombia — Sede Manizales · Programación de sistemas Linux embebido · Noviembre de 2026

Los integrantes declaran la participación y los aportes siguientes en el desarrollo del proyecto Osciloscopio.

> Los roles técnicos fueron definidos por el equipo. Cada integrante puede ampliar el detalle de sus aportes en la sección siguiente.

## Integrantes, roles y actividades

| # | Integrante | C.C. | Rol técnico | Actividades principales |
|---|---|---|---|---|
| 1 | Juan Felipe Pachón Restrepo | 1054542918 | **Ingeniero de Documentación Técnica y Gestión de Configuración** | Coordinación del repositorio; redacción y mantenimiento de la documentación (diagramas de bloque y de flujo, requerimientos, README); consolidación del video y de las declaraciones |
| 2 | Juan David Romero Calderón | 1122510542 | **Arquitecto de Sistemas Embebidos** | Arquitectura hardware/software; interfaces y protocolo SPI; estimación de tasas máximas de transferencia con las hojas de datos; restricciones y requerimientos |
| 3 | Sebastián Tovar Jiménez | 1104939220 | **Ingeniero de Software y Firmware Embebido** | Código y configuración de servicios en el SBC (servicio, Nginx, systemd); código y pruebas preliminares del firmware del ESP32-S3 (Zephyr) |
| 4 | Juan Pablo Vargas Córdoba | 1076904755 | **Ingeniero de Verificación y Validación (V&V)** | Diseño de experimentos y matriz de verificación; pruebas unitarias en SBC y microcontrolador; pruebas de integración; registro de resultados |

## Aportes detallados

### Juan Felipe Pachón Restrepo — Ingeniero de Documentación Técnica y Gestión de Configuración
- Coordinación del repositorio de Git y de su estructura según los entregables.
- Redacción y mantenimiento de la documentación: descripción del proyecto, diagramas de bloques y de flujo, requerimientos, instrucciones de instalación y README.
- Consolidación del video y de las declaraciones de participación y de uso de IA.

### Juan David Romero Calderón — Arquitecto de Sistemas Embebidos
- Definición de la arquitectura hardware/software (ESP32-S3 como esclavo SPI, Raspberry Pi 4 como maestro y servidor web).
- Definición de las interfaces, el cableado y el protocolo SPI.
- Estimación de las tasas máximas de transferencia con las hojas de datos; requerimientos y restricciones.

### Sebastián Tovar Jiménez — Ingeniero de Software y Firmware Embebido
- Firmware del ESP32-S3 en Zephyr: adquisición con el ADC, esclavo SPI, presets y monitor serie, y sus pruebas preliminares.
- Software de la Raspberry Pi: servicio de adquisición, configuración de Nginx y `systemd`, y dashboard web.

### Juan Pablo Vargas Córdoba — Ingeniero de Verificación y Validación (V&V)
- Diseño de experimentos y construcción de la matriz de verificación.
- Ejecución de las pruebas unitarias en el SBC y en el microcontrolador.
- Ejecución de las pruebas de integración y registro de los resultados.

## Apoyo de herramientas de inteligencia artificial

El uso de inteligencia artificial se declara en el [Documento 5](05_Declaracion_uso_IA.md), y el contenido de los chats está en el [Documento 6](06_Registro_conversaciones.md).
