# Documentación del Osciloscopio

| # | Documento | Contenido | Entregable que cubre |
|---|---|---|---|
| 1 | [Especificación técnica y arquitectura](Latex/01_Especificacion_tecnica_y_arquitectura.tex) | Descripción, diagrama de bloques, conexiones, decisiones de diseño, requerimientos y restricciones, estimación de la tasa SPI, diagramas de flujo | Descripción detallada · Requerimientos y restricciones |
| 2 | [Matriz de verificación y plan de pruebas](Latex/02_Matriz_verificacion_y_plan_de_pruebas.tex) | Estrategia, matriz, casos de prueba unitarios y de integración, registro de resultados, defectos encontrados | Matriz de verificación con resultados |
| 3 | [Protocolo SPI](Latex/03_Protocolo_SPI.tex) | Formato de trama v2 (y compatibilidad v1), comandos, presets | Descripción detallada |
| 4 | [Declaración de participación](Latex/04_Declaracion_participacion.tex) | Integrantes, roles técnicos y aportes | Declaración de participación |
| 5 | [Declaración de uso de IA](Latex/05_Declaracion_uso_IA.tex) | Modelo, tareas, verificación del resultado | Declaración de uso de IA |
| 6 | [Registro de conversaciones](Latex/06_Registro_conversaciones.tex) | Contenido de los chats con la IA | Declaración de uso de IA |
| 7 | [Guía de operación y pruebas manuales](Latex/07_Guia_operacion_y_pruebas_manuales.tex) | Puesta en marcha paso a paso, 18 pruebas para una persona, hoja de registro | Instrucciones · Matriz de verificación |

Otras carpetas:

- [`Resultados_Pruebas/`](Resultados_Pruebas/): evidencia de cada prueba ejecutada, nombrada con el identificador de la matriz.
- [`IA/`](IA/): `exportar_chats.ps1`, que copia las transcripciones completas de Claude Code a `IA/Transcripciones_crudas/`. Esa carpeta queda fuera de Git porque contiene rutas locales y metadatos que deben revisarse antes de publicarse.
- [`PDF/`](PDF/): destino de los PDF compilados.

## Compilar los PDF

Con TeX Live o MiKTeX y `latexmk`:
```
cd Latex
make          # genera ../PDF/*.pdf
```
Sin instalar nada: subir la carpeta `Latex/` a Overleaf junto con `../Imagenes/Circuito.png` y compilar cada documento con pdfLaTeX.

> Los documentos pasaron una revisión estática (entornos y llaves balanceados) pero todavía no se han compilado.
