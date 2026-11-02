/*
 * Osciloscopio de 2 canales - firmware ESP32-S3 (Zephyr).
 *
 * Hilos:
 *   acq      (prio 5)  : adquiere ventanas y las publica en el anillo de slots
 *   spi_link (prio 2)  : atiende al maestro SPI (Raspberry Pi)
 *   main     (prio 10) : monitor serial, 1 linea de estado y la ultima ventana
 *                        cada SERIAL_PERIOD_MS
 */
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>

#include "acq.h"
#include "protocol.h"
#include "spi_link.h"

#define SERIAL_PERIOD_MS 1000

K_THREAD_DEFINE(acq_tid, 2048, acq_thread_fn, NULL, NULL, NULL, 5, 0, -1);
K_THREAD_DEFINE(spi_tid, 3072, spi_link_thread_fn, NULL, NULL, NULL, 2, 0, -1);

static void print_window(void)
{
	static uint16_t w[OSC_WIN_SAMPLES * OSC_NCH];
	struct acq_stats as;
	struct spi_stats ss;

	acq_get_stats(&as);
	spi_link_get_stats(&ss);
	uint16_t wseq = acq_copy_last(w);

	printk("STAT preset=%u fs=%u Sa/s ventanas=%u err_adc=%u t_adc_read=%u us "
	       "spi_tx=%u spi_parcial=%u cmd_ok=%u cmd_bad=%u resync=%u crc_test=0x%04x\n",
	       as.preset, as.fs_hz, as.windows, as.errors, as.last_win_us,
	       ss.transactions, ss.partial, ss.cmd_ok, ss.cmd_bad, ss.resyncs,
	       osc_crc16((const uint8_t *)"123456789", 9));

	if (as.windows == 0) {
		return;
	}
	printk("WIN seq=%u CH1:", wseq);
	for (int i = 0; i < OSC_WIN_SAMPLES; i++) {
		printk(" %u", w[2 * i]);
	}
	printk("\nWIN seq=%u CH2:", wseq);
	for (int i = 0; i < OSC_WIN_SAMPLES; i++) {
		printk(" %u", w[2 * i + 1]);
	}
	printk("\n");
}

int main(void)
{
	printk("\n=== Osciloscopio ESP32-S3 (Zephyr) ===\n");

	if (acq_init() || spi_link_init()) {
		printk("Error de inicializacion\n");
		return 0;
	}
	k_thread_start(acq_tid);
	k_thread_start(spi_tid);

	printk("CRC-16 de \"123456789\" = 0x%04x (esperado 0x29b1)\n",
	       osc_crc16((const uint8_t *)"123456789", 9));
	printk("Monitor serie: teclas 0..3 cambian el preset (100/1000/10000/41667 Sa/s)\n");

	const struct device *con = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	int64_t next_print = k_uptime_get() + SERIAL_PERIOD_MS;

	for (;;) {
		unsigned char ch;

		while (device_is_ready(con) && uart_poll_in(con, &ch) == 0) {
			if (ch >= '0' && ch < '0' + OSC_NPRESETS) {
				acq_request_preset(ch - '0');
				printk("-> preset %c solicitado (%u Sa/s)\n", ch,
				       acq_preset_fs(ch - '0'));
			}
		}
		if (k_uptime_get() >= next_print) {
			next_print += SERIAL_PERIOD_MS;
			print_window();
		}
		k_msleep(20);
	}
	return 0;
}
