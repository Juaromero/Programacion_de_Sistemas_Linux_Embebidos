# Compilación de los documentos del Proyecto 2.
# Requiere una distribución LaTeX con babel-spanish, tikz y booktabs (TeX Live o MiKTeX).
#   make        -> genera los PDF en ../PDF
#   make clean  -> borra auxiliares

DOCS = 01_Especificacion_tecnica_y_arquitectura \
       02_Matriz_verificacion_y_plan_de_pruebas \
       03_Protocolo_SPI \
       04_Declaracion_participacion \
       05_Declaracion_uso_IA \
       06_Registro_conversaciones \
       07_Guia_operacion_y_pruebas_manuales

PDFS = $(addprefix ../PDF/,$(addsuffix .pdf,$(DOCS)))

all: $(PDFS)

../PDF/%.pdf: %.tex preambulo.tex portada.tex
	mkdir -p ../PDF
	latexmk -pdf -interaction=nonstopmode -halt-on-error -outdir=../PDF $<

clean:
	cd ../PDF && rm -f *.aux *.log *.out *.toc *.fls *.fdb_latexmk

.PHONY: all clean
