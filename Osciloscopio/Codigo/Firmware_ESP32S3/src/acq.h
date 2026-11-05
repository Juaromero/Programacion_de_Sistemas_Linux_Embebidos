/* Adquisicion de 2 canales (ADC1: GPIO1 y GPIO2) en ventanas de 64 muestras.
 * Cada canal tiene su propio preset y se puede activar o desactivar. */
#ifndef OSC_ACQ_H
#define OSC_ACQ_H

#include <stdint.h>
#include "protocol.h"

#define ACQ_BOTH 2u   /* indice de canal especial: aplicar a los dos */

struct acq_stats {
	uint32_t windows;       /* ventanas publicadas */
	uint32_t errors;        /* errores del ADC */
	uint32_t last_win_us;   /* duracion de la ultima adquisicion (incluye arranque) */
	struct osc_win_info info;  /* presets, fs, mascara y alineacion actuales */
};

int  acq_init(void);
void acq_request_preset(uint8_t ch, uint8_t preset);  /* ch: 0, 1 o ACQ_BOTH */
void acq_request_mask(uint8_t mask);                  /* bit0 = CH1, bit1 = CH2 */
uint8_t acq_get_mask(void);
uint32_t acq_preset_fs(uint8_t preset);
void acq_get_stats(struct acq_stats *out);
/* Copia la ultima ventana (para el monitor serie). Devuelve wseq. */
uint16_t acq_copy_last(uint16_t d1[OSC_WIN_SAMPLES], uint16_t d2[OSC_WIN_SAMPLES],
		       struct osc_win_info *info);
void acq_thread_fn(void *a, void *b, void *c);
void acq_dump_dma(void);   /* diagnostico: entradas DMA crudas de la ultima rafaga */

#endif
