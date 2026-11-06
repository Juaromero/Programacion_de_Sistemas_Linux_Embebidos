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

### Juan Felipe Pachón Restrepo — Documentación y gestión del repositorio
- Armé la estructura del repo y fui subiendo los cambios.
- Hice los documentos y los diagramas, y los fui corrigiendo cuando cambiaba algo del diseño.
- Junté el video y las declaraciones.

### Juan David Romero Calderón — Arquitectura
- Definí cómo se conectaban la ESP32 y la Raspberry y qué pines usar.
- Diseñé la trama SPI y calculé las tasas teóricas con las hojas de datos.
- Escribí los requerimientos y restricciones.

### Sebastián Tovar Jiménez — Software y firmware
- Programé la ESP32 en Zephyr (ADC, SPI esclavo y monitor serie).
- Hice el programa de la Raspberry, la página web y la configuración de Nginx y systemd.
- Corregí los errores que fueron saliendo en las pruebas.

### Juan Pablo Vargas Córdoba — Pruebas
- Armé la matriz de verificación y decidí qué probar y cómo.
- Hice las pruebas de la ESP32 y de la Raspberry por separado y después las de todo el sistema.
- Anoté los resultados y las evidencias de cada prueba.

## Apoyo de herramientas de inteligencia artificial

El uso de inteligencia artificial se declara en el [Documento 5](05_Declaracion_uso_IA.md), y el contenido de los chats está en el [Documento 6](06_Registro_conversaciones.md).
