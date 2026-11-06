# Documentación del Proyecto 2 — Osciloscopio

Todos los documentos están en Markdown y se leen directamente en GitHub (tablas, ecuaciones y diagramas Mermaid incluidos).

| # | Documento | Contenido | Entregable que cubre |
|---|---|---|---|
| 1 | [Especificación técnica y arquitectura](01_Especificacion_tecnica_y_arquitectura.md) | Descripción, diagrama de bloques, conexiones, decisiones de diseño, requerimientos y restricciones, estimación de la tasa SPI, diagramas de flujo | Descripción detallada · Requerimientos y restricciones |
| 2 | [Matriz de verificación y plan de pruebas](02_Matriz_verificacion_y_plan_de_pruebas.md) | Estrategia, matriz, casos de prueba unitarios y de integración, registro de resultados, defectos encontrados | Matriz de verificación con resultados |
| 3 | [Protocolo SPI](03_Protocolo_SPI.md) | Formato de trama v2 (y compatibilidad v1), comandos, presets | Descripción detallada |
| 4 | [Declaración de participación](04_Declaracion_participacion.md) | Integrantes, roles técnicos y aportes | Declaración de participación |
| 5 | [Declaración de uso de IA](05_Declaracion_uso_IA.md) | Modelo, tareas, verificación del resultado | Declaración de uso de IA |
| 6 | [Registro de conversaciones](06_Registro_conversaciones.md) | Contenido de los chats con la IA | Declaración de uso de IA |

Las instrucciones de instalación, configuración y uso están en [`../INSTALACION.md`](../INSTALACION.md).

Otras carpetas:

- [`Resultados_Pruebas/`](Resultados_Pruebas/): evidencia de cada prueba ejecutada, nombrada con el identificador de la matriz.
- [`IA/`](IA/): `exportar_chats.ps1`, que copia las transcripciones completas de Claude Code a `IA/Transcripciones_crudas/`. Esa carpeta queda fuera de Git porque contiene rutas locales y metadatos que deben revisarse antes de publicarse.
