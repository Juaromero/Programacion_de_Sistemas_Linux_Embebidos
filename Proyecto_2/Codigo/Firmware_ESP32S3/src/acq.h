/* Adquisicion de 2 canales (ADC1: GPIO1 y GPIO2) en ventanas de 64 muestras. */
#ifndef OSC_ACQ_H
#define OSC_ACQ_H

#include <stdint.h>
#include "protocol.h"

struct acq_stats {
	uint32_t windows;      /* ventanas adquiridas */
	uint32_t errors;       /* errores del ADC */
	uint32_t last_win_us;  /* duracion de la ultima adc_read (incluye arranque) */
	uint32_t fs_hz;        /* frecuencia de muestreo nominal activa */
	uint8_t  preset;       /* preset activo */
};

int  acq_init(void);
void acq_request_preset(uint8_t preset);   /* seguro desde cualquier hilo */
uint32_t acq_preset_fs(uint8_t preset);
void acq_get_stats(struct acq_stats *out);
/* Copia la ultima ventana (para el monitor serial). Devuelve wseq. */
uint16_t acq_copy_last(uint16_t pairs[OSC_WIN_SAMPLES * OSC_NCH]);
void acq_thread_fn(void *a, void *b, void *c);

#endif
