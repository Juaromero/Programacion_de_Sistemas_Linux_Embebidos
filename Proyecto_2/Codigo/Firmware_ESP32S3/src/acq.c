/*
 * Adquisicion del ADC1 del ESP32-S3.
 *
 * Los 2 canales NO se convierten en el mismo instante: el controlador digital
 * del ADC los convierte uno tras otro dentro del mismo "pattern". El desfase
 * entre CH1 y CH2 es 1 intervalo de conversion (12..500 us segun preset) y es
 * constante. Es la mejor opcion del chip: ADC1 es una unica SAR ADC
 * multiplexada, no hay dos ADC simultaneos.
 *
 * Modos:
 *  - MODE_DMA   : una rafaga DMA de 64 pares con intervalo_us constante entre
 *                 conversiones (temporizacion por hardware, sin jitter).
 *  - MODE_TIMED : para 100 Sa/s (por debajo del minimo de 611 Hz del
 *                 controlador digital): 1 par por tick de un temporizador
 *                 absoluto (sin deriva) y 64 ticks por ventana.
 */
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#include "acq.h"

enum acq_mode { MODE_TIMED, MODE_DMA };

struct preset {
	uint32_t fs_hz;       /* frecuencia de muestreo por canal (nominal) */
	enum acq_mode mode;
	uint32_t interval_us; /* DMA: us entre conversiones consecutivas */
	uint32_t period_us;   /* TIMED: periodo del par de muestras */
};

/* Conversiones por segundo (DMA) = 1e6/interval_us, repartidas entre 2 canales:
 * fs por canal = 1e6 / (2*interval_us). Limite del ADC: 83333 conv/s. */
static const struct preset presets[OSC_NPRESETS] = {
	{ 100,   MODE_TIMED, 0,   10000 },
	{ 1000,  MODE_DMA,   500, 0 },
	{ 10000, MODE_DMA,   50,  0 },
	{ 41667, MODE_DMA,   12,  0 },  /* bonificacion: >= 40 kSa/s por canal */
};

static const struct adc_dt_spec adc_ch[2] = {
	ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0),
	ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1),
};

static atomic_t req_preset = ATOMIC_INIT(0);
static struct acq_stats stats;
static uint16_t pairs[OSC_WIN_SAMPLES * OSC_NCH];
static uint16_t last_pairs[OSC_WIN_SAMPLES * OSC_NCH];
static uint16_t last_wseq;
static K_MUTEX_DEFINE(last_lock);

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

void acq_request_preset(uint8_t preset)
{
	if (preset < OSC_NPRESETS) {
		atomic_set(&req_preset, preset);
	}
}

uint32_t acq_preset_fs(uint8_t preset)
{
	return preset < OSC_NPRESETS ? presets[preset].fs_hz : 0;
}

void acq_get_stats(struct acq_stats *out)
{
	*out = stats;
}

uint16_t acq_copy_last(uint16_t out[OSC_WIN_SAMPLES * OSC_NCH])
{
	uint16_t w;

	k_mutex_lock(&last_lock, K_FOREVER);
	memcpy(out, last_pairs, sizeof(last_pairs));
	w = last_wseq;
	k_mutex_unlock(&last_lock);
	return w;
}

static int read_dma_window(const struct preset *p)
{
	struct adc_sequence_options opt = {
		.interval_us = p->interval_us,
		.extra_samplings = OSC_WIN_SAMPLES - 1,
	};
	struct adc_sequence seq = {
		.options = &opt,
		.channels = BIT(adc_ch[0].channel_id) | BIT(adc_ch[1].channel_id),
		.buffer = pairs,
		.buffer_size = sizeof(pairs),
		.resolution = OSC_ADC_BITS,
	};
	uint32_t t0 = k_cycle_get_32();
	int err = adc_read(adc_ch[0].dev, &seq);

	stats.last_win_us = k_cyc_to_us_floor32(k_cycle_get_32() - t0);
	return err;
}

static int read_timed_window(const struct preset *p)
{
	struct adc_sequence seq = {
		.channels = BIT(adc_ch[0].channel_id) | BIT(adc_ch[1].channel_id),
		.buffer_size = 2 * sizeof(uint16_t),
		.resolution = OSC_ADC_BITS,
	};
	int64_t next = k_uptime_ticks();
	int64_t step = k_us_to_ticks_ceil64(p->period_us);
	uint32_t t0 = k_cycle_get_32();

	for (uint32_t i = 0; i < OSC_WIN_SAMPLES; i++) {
		next += step;
		k_sleep(K_TIMEOUT_ABS_TICKS(next));
		seq.buffer = &pairs[2 * i];
		int err = adc_read(adc_ch[0].dev, &seq);

		if (err) {
			return err;
		}
	}
	stats.last_win_us = k_cyc_to_us_floor32(k_cycle_get_32() - t0);
	return 0;
}

void acq_thread_fn(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
	uint16_t wseq = 0;

	for (;;) {
		uint8_t pi = (uint8_t)atomic_get(&req_preset);
		const struct preset *p = &presets[pi];   /* cambio efectivo por ventana */
		int err;

		stats.preset = pi;
		stats.fs_hz = p->fs_hz;

		err = (p->mode == MODE_DMA) ? read_dma_window(p) : read_timed_window(p);
		if (err) {
			stats.errors++;
			k_msleep(10);
			continue;
		}

		int s = osc_slot_get_free();

		if (s >= 0) {
			osc_slot_fill(osc_slot_ptr(s), pairs, p->fs_hz, wseq, pi);
			osc_slot_publish(s);
		}

		k_mutex_lock(&last_lock, K_FOREVER);
		memcpy(last_pairs, pairs, sizeof(pairs));
		last_wseq = wseq;
		k_mutex_unlock(&last_lock);

		wseq++;
		stats.windows++;
	}
}
