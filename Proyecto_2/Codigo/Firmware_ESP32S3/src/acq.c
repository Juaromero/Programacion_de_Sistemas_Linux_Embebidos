/*
 * Adquisicion del ADC1 del ESP32-S3 con muestreo independiente por canal.
 *
 * El ADC1 es una unica SAR ADC multiplexada: no hay dos conversores. Por eso:
 *
 *  - CANALES SINCRONIZADOS: si los dos canales estan activos con el MISMO
 *    preset, se adquieren en una sola rafaga DMA con el patron CH1,CH2. CH2 se
 *    convierte un intervalo despues de CH1 (desfase constante y conocido).
 *  - CANALES INDEPENDIENTES: si tienen presets distintos (o el preset no admite
 *    dos canales en rafaga), cada canal se adquiere en su propia rafaga, uno
 *    detras del otro, cada uno a su frecuencia. Las dos ventanas no estan
 *    alineadas en el tiempo.
 *  - CANAL APAGADO: no se adquiere; el otro canal se refresca mas rapido.
 *
 * Modos por preset:
 *  - MODE_DMA   : rafaga DMA de 64 muestras con intervalo constante entre
 *                 conversiones (temporizacion por hardware).
 *  - MODE_TIMED : 100 Sa/s, por debajo del minimo de 611 conv/s del
 *                 controlador digital: 1 lectura por tick de un temporizador
 *                 absoluto (sin deriva).
 *
 * La primera conversion de cada rafaga DMA vale 0 (medido): se adquiere una
 * muestra extra por canal al inicio y se descarta.
 */
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#include "acq.h"
#include "adc_esp32.h"   /* struct adc_esp32_data: acceso al bufer DMA crudo */

enum acq_mode { MODE_TIMED, MODE_DMA };

struct preset {
	uint32_t fs_hz;      /* frecuencia de muestreo por canal (nominal) */
	enum acq_mode mode;
	uint32_t pair_us;    /* DMA, 2 canales en rafaga: us entre conversiones (0 = no admitido) */
	uint32_t single_us;  /* DMA, 1 canal: us entre conversiones */
	uint32_t period_us;  /* TIMED: periodo de muestreo */
};

/* Limite del controlador digital: 611 a 83 333 conversiones por segundo.
 * Dos canales en rafaga: fs = 1e6 / (2 * pair_us). Un canal: fs = 1e6 / single_us. */
static const struct preset presets[OSC_NPRESETS] = {
	{ 100,   MODE_TIMED, 0,   0,    10000 },
	{ 1000,  MODE_DMA,   500, 1000, 0 },
	{ 10000, MODE_DMA,   50,  100,  0 },
	{ 41667, MODE_DMA,   12,  24,   0 },  /* bonificacion: >= 40 kSa/s por canal */
	{ 83333, MODE_DMA,   0,   12,   0 },  /* maximo del ADC: un canal por rafaga */
};

static const struct adc_dt_spec adc_ch[2] = {
	ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0),
	ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1),
};

#define DISCARD 1   /* muestras descartadas al inicio de cada rafaga, por canal */

static atomic_t req_preset[2] = { ATOMIC_INIT(0), ATOMIC_INIT(0) };
static atomic_t req_mask = ATOMIC_INIT(0x3);
static struct acq_stats stats;
static uint16_t raw[(OSC_WIN_SAMPLES + DISCARD) * OSC_NCH];
static uint16_t data[OSC_NCH][OSC_WIN_SAMPLES];
static uint16_t last_data[OSC_NCH][OSC_WIN_SAMPLES];
static struct osc_win_info last_info;
static uint16_t last_wseq;
static K_MUTEX_DEFINE(last_lock);

/* Diagnostico: entradas de la ultima rafaga DMA cruda (tecla 'd' del monitor serie) */
static volatile uint32_t dbg_entries;

void acq_dump_dma(void)
{
	const struct adc_esp32_data *ad = adc_ch[0].dev->data;
	const adc_digi_output_data_t *e = (const adc_digi_output_data_t *)ad->dma_buffer;
	uint32_t n = dbg_entries, valid[2] = { 0, 0 }, invalid = 0;

	for (uint32_t i = 0; i < n; i++) {
		if (e[i].type2.unit == 0 && e[i].type2.channel < 2) {
			valid[e[i].type2.channel]++;
		} else {
			invalid++;
		}
	}
	uint32_t first_nz = n, zeros_after = 0, zeros_total = 0;

	for (uint32_t i = 0; i < n; i++) {
		if (e[i].type2.data == 0) {
			zeros_total++;
			if (first_nz < n) {
				zeros_after++;
			}
		} else if (first_nz == n) {
			first_nz = i;
		}
	}
	printk("DMA entradas=%u validas c0=%u c1=%u invalidas=%u | ceros=%u, primer dato!=0 en %u, ceros despues=%u\n",
	       n, valid[0], valid[1], invalid, zeros_total, first_nz, zeros_after);
}

int acq_init(void)
{
	for (int i = 0; i < 2; i++) {
		if (!adc_is_ready_dt(&adc_ch[i])) {
			printk("ADC canal %d no listo\n", i);
			return -ENODEV;
		}
		int err = adc_channel_setup_dt(&adc_ch[i]);

		if (err) {
			printk("adc_channel_setup canal %d: %d\n", i, err);
			return err;
		}
	}
	return 0;
}

void acq_request_preset(uint8_t ch, uint8_t preset)
{
	if (preset >= OSC_NPRESETS) {
		return;
	}
	if (ch == 0 || ch == ACQ_BOTH) {
		atomic_set(&req_preset[0], preset);
	}
	if (ch == 1 || ch == ACQ_BOTH) {
		atomic_set(&req_preset[1], preset);
	}
}

void acq_request_mask(uint8_t mask)
{
	atomic_set(&req_mask, mask & 0x3);
}

uint8_t acq_get_mask(void)
{
	return (uint8_t)atomic_get(&req_mask);
}

uint32_t acq_preset_fs(uint8_t preset)
{
	return preset < OSC_NPRESETS ? presets[preset].fs_hz : 0;
}

void acq_get_stats(struct acq_stats *out)
{
	*out = stats;
}

uint16_t acq_copy_last(uint16_t d1[OSC_WIN_SAMPLES], uint16_t d2[OSC_WIN_SAMPLES],
		       struct osc_win_info *info)
{
	uint16_t w;

	k_mutex_lock(&last_lock, K_FOREVER);
	memcpy(d1, last_data[0], sizeof(last_data[0]));
	memcpy(d2, last_data[1], sizeof(last_data[1]));
	*info = last_info;
	w = last_wseq;
	k_mutex_unlock(&last_lock);
	return w;
}

/* Adquiere 64 muestras de los canales de chmask (1, 2 o 3) con el preset p.
 * interval_us solo se usa en MODE_DMA. Los datos quedan en data[c][]. */
static int read_window(uint8_t chmask, const struct preset *p, uint32_t interval_us)
{
	const uint32_t nch = (uint32_t)__builtin_popcount(chmask);
	uint32_t channels = 0;

	for (int c = 0; c < 2; c++) {
		if (chmask & BIT(c)) {
			channels |= BIT(adc_ch[c].channel_id);
		}
	}

	struct adc_sequence_options opt = { 0 };
	struct adc_sequence seq = {
		.options = &opt,
		.channels = channels,
		.buffer = raw,
		.resolution = OSC_ADC_BITS,
	};

	if (p->mode == MODE_DMA) {
		opt.interval_us = interval_us;
		opt.extra_samplings = OSC_WIN_SAMPLES + DISCARD - 1;
		seq.buffer_size = (OSC_WIN_SAMPLES + DISCARD) * nch * sizeof(uint16_t);
		dbg_entries = (OSC_WIN_SAMPLES + DISCARD) * nch;

		int err = adc_read(adc_ch[0].dev, &seq);

		if (err) {
			return err;
		}
		/* El controlador convierte en orden ascendente de canal: CH1, CH2 */
		for (uint32_t i = 0; i < OSC_WIN_SAMPLES; i++) {
			uint32_t k = 0;

			for (int c = 0; c < 2; c++) {
				if (chmask & BIT(c)) {
					data[c][i] = raw[(i + DISCARD) * nch + k++];
				}
			}
		}
		return 0;
	}

	/* MODE_TIMED: en cada tick una rafaga corta; se usa la ultima muestra */
	opt.extra_samplings = DISCARD;
	seq.buffer_size = (DISCARD + 1) * nch * sizeof(uint16_t);
	dbg_entries = (DISCARD + 1) * nch;

	int64_t next = k_uptime_ticks();
	const int64_t step = k_us_to_ticks_ceil64(p->period_us);

	for (uint32_t i = 0; i < OSC_WIN_SAMPLES; i++) {
		next += step;
		k_sleep(K_TIMEOUT_ABS_TICKS(next));

		int err = adc_read(adc_ch[0].dev, &seq);

		if (err) {
			return err;
		}
		uint32_t k = 0;

		for (int c = 0; c < 2; c++) {
			if (chmask & BIT(c)) {
				data[c][i] = raw[DISCARD * nch + k++];
			}
		}
	}
	return 0;
}

void acq_thread_fn(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
	uint16_t wseq = 0;

	for (;;) {
		const uint8_t mask = (uint8_t)atomic_get(&req_mask) & 0x3;
		const uint8_t pr[2] = { (uint8_t)atomic_get(&req_preset[0]),
					(uint8_t)atomic_get(&req_preset[1]) };
		struct osc_win_info info = {
			.fs = { presets[pr[0]].fs_hz, presets[pr[1]].fs_hz },
			.preset = { pr[0], pr[1] },
			.mask = mask,
			.aligned = 0,
		};
		const struct preset *p0 = &presets[pr[0]];
		uint32_t t0 = k_cycle_get_32();
		int err = 0;

		if (mask == 0) {
			memset(data, 0, sizeof(data));
			k_msleep(100);              /* nada que adquirir: publicar estado */
		} else if (mask == 0x3 && pr[0] == pr[1] &&
			   (p0->mode == MODE_TIMED || p0->pair_us > 0)) {
			err = read_window(0x3, p0, p0->pair_us);   /* sincronizados */
			info.aligned = 1;
		} else {
			for (int ch = 0; ch < 2 && !err; ch++) {   /* independientes */
				if (mask & BIT(ch)) {
					err = read_window(BIT(ch), &presets[pr[ch]],
							  presets[pr[ch]].single_us);
				} else {
					memset(data[ch], 0, sizeof(data[ch]));
				}
			}
		}
		stats.last_win_us = k_cyc_to_us_floor32(k_cycle_get_32() - t0);
		stats.info = info;

		if (err) {
			stats.errors++;
			k_msleep(10);
			continue;
		}

		int s = osc_slot_get_free();

		if (s >= 0) {
			osc_slot_fill(osc_slot_ptr(s), data[0], data[1], &info, wseq);
			osc_slot_publish(s);
		}

		k_mutex_lock(&last_lock, K_FOREVER);
		memcpy(last_data, data, sizeof(data));
		last_info = info;
		last_wseq = wseq;
		k_mutex_unlock(&last_lock);

		wseq++;
		stats.windows++;
	}
}
