# Documento 4 — Declaración de participación y aportes

**Sistema Híbrido de Adquisición Analógica mediante SPI y Dashboard Embebido en Linux**
Universidad Nacional de Colombia — Sede Manizales · Programación de sistemas Linux embebido · Noviembre de 2026

Los integrantes declaran la participación y los aportes siguientes en el desarrollo del proyecto Osciloscopio.

> Los roles técnicos fueron definidos por el equipo. Cada integrante puede ampliar el detalle de sus aportes en la sección siguiente.

## Integrantes, roles y actividades

| # | Integrante | C.C. | Rol técnico | Actividades principales |
|---|---|---|---|---|
| 1 | Juan Felipe Pachón Restrepo | 1054542918 | **Ingeniero de Documentación Técnica y Gestión de Configuración** | Coordinación del repositorio; primera versión y revisión de la documentación; consolidación del video y de las declaraciones |
| 2 | Juan David Romero Calderón | 1122510542 | **Arquitecto de Sistemas Embebidos** | Arquitectura del sistema; elección de las placas; asignación de pines, cableado y circuito; decisiones generales de diseño |
| 3 | Sebastián Tovar Jiménez | 1104939220 | **Ingeniero de Software y Firmware Embebido** | Entorno de desarrollo (Zephyr en WSL, Raspberry Pi por SSH y UART); carga del firmware y puesta en marcha de los programas en las dos placas |
| 4 | Juan Pablo Vargas Córdoba | 1076904755 | **Ingeniero de Verificación y Validación (V&V)** | Primera versión de la matriz y del plan de pruebas; pruebas con el sistema armado; revisión de los resultados |

## Aportes detallados

Cada uno cuenta con sus palabras lo que hizo. La parte más pesada de la programación, el diseño detallado del protocolo y los cálculos se apoyaron en una herramienta de IA, como se explica en el [Documento 5](05_Declaracion_uso_IA.md); lo que sigue es el trabajo que hicimos nosotros.

### Juan Felipe Pachón Restrepo — Documentación y gestión del repositorio

Me encargué de que el proyecto quedara ordenado y entendible para cualquiera que lo abra. Al principio redacté con el equipo la primera versión de los documentos, la especificación y la matriz de verificación, con lo que pedía el enunciado del profesor. Después fui revisando cada documento a medida que el proyecto cambiaba (por ejemplo cuando pasamos a manejar los canales por separado o cuando cambiamos de placa), para que lo escrito coincidiera con lo que de verdad funcionaba. También estuve pendiente de que el repositorio tuviera todo lo que se pedía en los entregables, de que la documentación se pudiera leer directamente en GitHub sin tener que compilar nada, y de reunir el video y las declaraciones al final.

### Juan David Romero Calderón — Arquitectura

Mi parte fue decidir cómo iba a estar armado el sistema y que las piezas encajaran. Con el equipo elegimos usar la Raspberry Pi 4 como servidor y la ESP32-S3 para medir, y que la comunicación entre las dos fuera por SPI, como pedía el enunciado. Definí qué pines de la ESP32 iban a qué pines de la Raspberry y cuáles se usaban para los dos canales, y dibujé el circuito en Fritzing para que todos tuviéramos la misma referencia al conectar. Cuando cambiamos de placa a la ESP32-S3-DevKitC-1 revisé otra vez el diagrama de pines para comprobar que las conexiones seguían siendo correctas. También fui quien tomó varias de las decisiones generales del diseño, como que la captura fuera por refresco de ventanas y no bajo demanda, que los datos se vieran también en el monitor serie, y qué servidor web y lenguaje usar en la Raspberry.

### Sebastián Tovar Jiménez — Software y firmware

Me tocó la parte de poner a correr los programas en las dos placas. Preparé el entorno de trabajo: Zephyr instalado en WSL para programar la ESP32 y la Raspberry con su sistema, su conexión por SSH y la consola por UART con el adaptador CP2102. Con eso cargué el firmware en la ESP32 cada vez que hubo una versión nueva, revisé en el monitor serie que estuviera midiendo, y comprobé en la Raspberry que el servicio y la página web arrancaran y se vieran bien en el navegador. Cuando algo no funcionaba (la placa no aparecía en WSL, el puerto serie ocupado o la Raspberry sin red) era el que buscaba qué estaba pasando y lo dejaba andando otra vez.

### Juan Pablo Vargas Córdoba — Pruebas

Yo estuve del lado de comprobar que todo funcionara de verdad. Participé en la primera versión de la matriz de verificación y del plan de pruebas, pensando qué había que medir para saber si cumplíamos cada requisito. Después, con el sistema armado, conecté las señales de prueba a los canales, cambié los presets y los canales desde la página, miré que las mediciones tuvieran sentido y anoté qué pasaba en cada caso. Revisé los resultados que quedaron en la matriz para que lo aprobado estuviera realmente comprobado, y que lo que no pudimos medir (por ejemplo lo que necesita un analizador lógico) quedara como pendiente y no como aprobado.

## Apoyo de herramientas de inteligencia artificial

El uso de inteligencia artificial se declara en el [Documento 5](05_Declaracion_uso_IA.md), y el contenido de los chats está en el [Documento 6](06_Registro_conversaciones.md).
