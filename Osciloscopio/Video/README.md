# Video del funcionamiento

> **Pendiente de grabar.** Cuando esté listo, subirlo (YouTube, Google Drive o similar) y reemplazar esta línea por el enlace:
>
> **Enlace al video:** _(pendiente)_

Los archivos de video pesados no se suben a Git; si se quiere conservar una copia local, colocarla en esta carpeta y agregarla al `.gitignore`.

## Guion sugerido (3 a 5 minutos)

| # | Escena | Qué mostrar |
|---|---|---|
| 1 | Presentación | Integrantes, objetivo del proyecto y diagrama de bloques (Documento 1) |
| 2 | Montaje | Cableado SPI ESP32-S3 ↔ Raspberry Pi (pines 24/19/23/21 + GND) y las entradas CH1/CH2 |
| 3 | Arranque | Encender la Raspberry Pi y el ESP32-S3; `systemctl status oscd nginx` por SSH o por la consola UART |
| 4 | Monitor serie | Líneas `STAT` y `WIN` del ESP32-S3; cambio de preset con las teclas |
| 5 | Dashboard | Abrir `http://<ip>/`, indicador «en vivo», señal de prueba en ambos canales |
| 6 | Presets y canales | Cambiar presets, apagar un canal, canales independientes y botón «Sincronizar» |
| 7 | Bonificación | Preset de 41,7 kSa/s (y 83,3 kSa/s con un canal) con una señal de ~1 kHz; mediciones de frecuencia |
| 8 | Muestreo y aliasing | Una señal muestreada con pocas muestras por período: aviso rojo y botón «Aplicar» |
| 9 | Robustez | Desconectar el ESP32-S3 y reconectarlo: el dashboard se recupera solo |
| 10 | Cierre | Matriz de verificación (Documento 2) y resultados principales |

Señal de prueba sin generador: desde la Raspberry Pi, `gpioset -c gpiochip0 -t 500us GPIO18=1` (onda cuadrada de 1 kHz en el pin 12) conectada a CH1 a través de 1 kΩ.
