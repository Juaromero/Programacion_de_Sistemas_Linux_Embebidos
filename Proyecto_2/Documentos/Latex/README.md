# Documentos LaTeX del Proyecto 2

| Archivo | Contenido |
|---|---|
| `preambulo.tex` | Paquetes, estilos TikZ y datos comunes (universidad, materia, título, fecha) |
| `portada.tex` | Comando `\portada{Título}` con la tabla de integrantes y roles técnicos |
| `01_Especificacion_tecnica_y_arquitectura.tex` | Documento 1: descripción, decisiones, requerimientos, protocolo, estimación SPI, diagramas de flujo |
| `02_Matriz_verificacion_y_plan_de_pruebas.tex` | Documento 2: estrategia, matriz de verificación, casos de prueba y registro de resultados |
| `03_Protocolo_SPI.tex` | Documento 3: formato de trama y comandos |
| `04_Declaracion_participacion.tex` | Documento 4: roles y aportes (completar nombres) |
| `05_Declaracion_uso_IA.tex` | Documento 5: declaración de uso de IA |
| `06_Registro_conversaciones.tex` | Documento 6: registro de chats |
| `07_Guia_operacion_y_pruebas_manuales.tex` | Documento 7: puesta en marcha paso a paso y pruebas de funcionamiento para una persona, con hoja de registro |

Para cambiar nombres de integrantes, fecha o título del proyecto basta editar `preambulo.tex` y `portada.tex`.

## Compilar

```
make            # requiere latexmk y TeX Live/MiKTeX; genera ../PDF/*.pdf
```

Sin instalar nada: subir esta carpeta completa a Overleaf (compilador pdfLaTeX) y compilar cada documento.

> Estado: estos documentos aún **no se han compilado** (no había distribución LaTeX en el equipo de trabajo). Si aparece algún error de compilación, repórtelo para corregirlo.
