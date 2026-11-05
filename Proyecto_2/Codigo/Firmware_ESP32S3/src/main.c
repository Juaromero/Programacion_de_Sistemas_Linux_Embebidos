/*
 * Osciloscopio de 2 canales - firmware ESP32-S3 (Zephyr).
 *
 * Hilos:
 *   acq      (prio 5)  : adquiere ventanas y las publica en el anillo de slots
 *   spi_link (prio 2)  : atiende al maestro SPI (Raspberry Pi)
 *   main     (prio 10) : monitor serie: 1 linea de estado y la ultima ventana
 *                        cada SERIAL_PERIOD_MS; teclas de control
 *
 * Teclas del monitor serie:
 *   0..4  preset de los dos canales (100, 1k, 10k, 41.7k, 83.3k Sa/s)
 *   q / w activar o desactivar CH1 / CH2
 *   a / s siguiente preset solo de CH1 / CH2
 */
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>

#include "acq.h"
#include "protocol.h"
#include "spi_link.h"

#define SERIAL_PERIOD_MS 1000

K_THREAD_DEFINE(acq_tid, 2048, acq_thread_fn, NULL, NULL, NULL, 5, 0, -1);
K_THREAD_DEFINE(spi_tid, 3072, spi_link_thread_fn, NULL, NULL, NULL, 2, 0, -1);

static void print_channel(int c, uint16_t wseq, const uint16_t *d, uint32_t fs)
{
	printk("WIN seq=%u CH%d fs=%u:", wseq, c + 1, fs);
	for (int i = 0; i < OSC_WIN_SAMPLES; i++) {
		printk(" %u", d[i]);
	}
	printk("\n");
}

static void print_window(void)
{
	static uint16_t d1[OSC_WIN_SAMPLES], d2[OSC_WIN_SAMPLES];
	struct osc_win_info wi;
	struct acq_stats as;
	struct spi_stats ss;

	acq_get_stats(&as);
	spi_link_get_stats(&ss);
	uint16_t wseq = acq_copy_last(d1, d2, &wi);

	printk("STAT canales=%s%s ch1=%u Sa/s ch2=%u Sa/s modo=%s ventanas=%u err_adc=%u "
	       "t_adq=%u us spi_tx=%u spi_parcial=%u cmd_ok=%u cmd_bad=%u crc_test=0x%04x\n",
	       (as.info.mask & 1) ? "CH1 " : "", (as.info.mask & 2) ? "CH2" : "",
	       as.info.fs[0], as.info.fs[1],
	       as.info.mask == 0 ? "apagado" : (as.info.aligned ? "sincronizado" : "independiente"),
	       as.windows, as.errors, as.last_win_us,
	       ss.transactions, ss.partial, ss.cmd_ok, ss.cmd_bad,
	       osc_crc16((const uint8_t *)"123456789", 9));

	if (as.windows == 0) {
		return;
	}
	if (wi.mask & 1) {
		print_channel(0, wseq, d1, wi.fs[0]);
	}
	if (wi.mask & 2) {
		print_channel(1, wseq, d2, wi.fs[1]);
	}
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
	printk("Teclas: 0..4 preset de ambos canales; q/w on/off CH1/CH2; a/s preset de CH1/CH2\n");

	const struct device *con = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	int64_t next_print = k_uptime_get() + SERIAL_PERIOD_MS;

	for (;;) {
		unsigned char ch;

		while (device_is_ready(con) && uart_poll_in(con, &ch) == 0) {
			struct acq_stats as;

			acq_get_stats(&as);
			if (ch >= '0' && ch < '0' + OSC_NPRESETS) {
				acq_request_preset(ACQ_BOTH, ch - '0');
				printk("-> preset %c en ambos canales (%u Sa/s)\n", ch,
				       acq_preset_fs(ch - '0'));
			} else if (ch == 'q' || ch == 'w') {
				uint8_t bit = (ch == 'q') ? 1 : 2;
				uint8_t m = acq_get_mask() ^ bit;

				acq_request_mask(m);
				printk("-> canales activos: %s%s\n", (m & 1) ? "CH1 " : "",
				       (m & 2) ? "CH2" : (m ? "" : "ninguno"));
			} else if (ch == 'd') {
				acq_dump_dma();
			} else if (ch == 'a' || ch == 's') {
				int c = (ch == 'a') ? 0 : 1;
				uint8_t p = (as.info.preset[c] + 1) % OSC_NPRESETS;

				acq_request_preset(c, p);
				printk("-> CH%d: %u Sa/s\n", c + 1, acq_preset_fs(p));
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
