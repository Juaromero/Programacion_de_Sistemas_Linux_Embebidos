#ifndef OSC_SPI_MASTER_H
#define OSC_SPI_MASTER_H

#include <stdint.h>
#include "protocol.h"

struct spi_master {
	int fd;
	uint32_t speed_hz;
	uint32_t gap_us;     /* pausa minima entre transacciones (rearme del esclavo) */
	uint64_t last_end_ns;
};

int  spim_open(struct spi_master *m, const char *dev, uint32_t speed_hz, uint32_t gap_us);
void spim_close(struct spi_master *m);
/* Una transaccion de 64 bytes (CS activo durante toda la trama).
 * Devuelve 0 y el tiempo del ioctl en ns, o -1. */
int  spim_xfer(struct spi_master *m, const uint8_t tx[OSC_SEG_LEN],
	       uint8_t rx[OSC_SEG_LEN], uint64_t *ioctl_ns);

uint64_t now_ns(void);
void sleep_until_ns(uint64_t t_ns);

#endif
