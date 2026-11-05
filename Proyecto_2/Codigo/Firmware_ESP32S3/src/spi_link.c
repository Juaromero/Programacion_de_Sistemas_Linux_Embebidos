/*
 * Esclavo SPI (spi2, modo 0, 8 bits). Cada transaccion es de 64 bytes:
 *   MISO: segmento de la ventana (ver protocol.h)
 *   MOSI: comando de la RPi (primeros 8 bytes), el resto se ignora
 * El esclavo no puede iniciar transferencias: despues de cada transaccion
 * prepara el siguiente segmento y se rearma. La RPi debe respetar un tiempo
 * minimo entre transacciones (gap) para que el esclavo alcance a rearmarse.
 *
 * Una ventana se sirve completa (5 segmentos) desde un unico slot "congelado",
 * asi que no se envia una ventana nueva hasta terminar de entregar la actual
 * (RNF-06). Al terminar, el siguiente ciclo toma la ventana mas reciente.
 */
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/sys/atomic.h>

#include "spi_link.h"
#include "protocol.h"
#include "acq.h"

static const struct device *const spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi2));

static const struct spi_config spi_cfg = {
	.operation = SPI_OP_MODE_PERIPHERAL | SPI_WORD_SET(8) | SPI_TRANSFER_MSB,
};

static struct spi_stats stats;
static uint8_t cmd_ack;       /* ultimo cmd_seq ejecutado */
static uint8_t tx_buf[OSC_SEG_LEN];
static uint8_t rx_buf[OSC_SEG_LEN];

int spi_link_init(void)
{
	if (!device_is_ready(spi_dev)) {
		printk("SPI esclavo no listo\n");
		return -ENODEV;
	}
	return 0;
}

void spi_link_get_stats(struct spi_stats *out)
{
	*out = stats;
}

static void handle_command(bool *resync)
{
	uint8_t cmd, arg, seq;

	if (osc_cmd_parse(rx_buf, &cmd, &arg, &seq) != 0) {
		/* Un maestro que no envia comando (MOSI = 0x00) no es un error */
		if (rx_buf[0] != 0x00 && rx_buf[0] != 0xFF) {
			stats.cmd_bad++;
		}
		return;
	}
	stats.cmd_ok++;
	if (cmd == OSC_CMD_NOP || seq == cmd_ack) {
		return; /* sin comando o repeticion de uno ya ejecutado */
	}
	if (cmd == OSC_CMD_SET_PRESET) {
		acq_request_preset(ACQ_BOTH, arg);         /* los dos canales */
		cmd_ack = seq;
	} else if (cmd == OSC_CMD_SET_CH_PRESET) {
		acq_request_preset((arg >> 4) & 0x1, arg & 0x0F);
		cmd_ack = seq;
	} else if (cmd == OSC_CMD_SET_MASK) {
		acq_request_mask(arg & 0x3);
		cmd_ack = seq;
	} else if (cmd == OSC_CMD_RESYNC) {
		*resync = true;
		stats.resyncs++;
		cmd_ack = seq;
	}
}

void spi_link_thread_fn(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
	uint8_t next_seg = 0;
	int slot = -1;

	for (;;) {
		if (next_seg == 0) {
			/* Inicio de ciclo: congelar la ventana mas reciente */
			osc_slot_unlock();
			slot = osc_slot_lock_latest();
		}
		osc_segment_build(tx_buf, slot >= 0 ? osc_slot_ptr(slot) : NULL,
				  next_seg, cmd_ack);

		const struct spi_buf txb = { .buf = tx_buf, .len = OSC_SEG_LEN };
		const struct spi_buf rxb = { .buf = rx_buf, .len = OSC_SEG_LEN };
		const struct spi_buf_set txs = { .buffers = &txb, .count = 1 };
		const struct spi_buf_set rxs = { .buffers = &rxb, .count = 1 };

		int n = spi_transceive(spi_dev, &spi_cfg, &txs, &rxs);

		if (n < 0 || n < (int)OSC_SEG_LEN) {
			stats.partial++;
			continue; /* no avanzar: se reintenta el mismo segmento */
		}
		stats.transactions++;

		bool resync = false;

		handle_command(&resync);
		if (resync) {
			next_seg = 0;
		} else {
			next_seg = (next_seg + 1) % OSC_NSEG;
		}
	}
}
