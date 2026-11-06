/* Enlace SPI esclavo (ESP32-S3) con la Raspberry Pi como maestro. */
#ifndef OSC_SPI_LINK_H
#define OSC_SPI_LINK_H

#include <stdint.h>

struct spi_stats {
	uint32_t transactions;  /* transacciones de 64 bytes completas */
	uint32_t partial;       /* transacciones incompletas */
	uint32_t cmd_ok;
	uint32_t cmd_bad;       /* comando con sync/CRC invalido (excluye NOP vacio) */
	uint32_t resyncs;
};

int  spi_link_init(void);
void spi_link_get_stats(struct spi_stats *out);
void spi_link_thread_fn(void *a, void *b, void *c);

#endif
