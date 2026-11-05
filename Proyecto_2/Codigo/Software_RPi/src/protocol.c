#include <string.h>
#include "protocol.h"

uint16_t osc_crc16(const uint8_t *data, size_t len)
{
	static uint16_t table[256];
	static int init;

	if (!init) {
		for (int i = 0; i < 256; i++) {
			uint16_t c = (uint16_t)(i << 8);

			for (int b = 0; b < 8; b++) {
				c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
			}
			table[i] = c;
		}
		init = 1;
	}
	uint16_t crc = 0xFFFF;

	while (len--) {
		crc = (uint16_t)((crc << 8) ^ table[(crc >> 8) ^ *data++]);
	}
	return crc;
}

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
	       ((uint32_t)p[3] << 24);
}

int osc_seg_parse(const uint8_t rx[OSC_SEG_LEN], struct osc_seg *out)
{
	if (rx[0] != OSC_SYNC_TX) {
		return SEG_BAD_SYNC;
	}
	if (osc_crc16(rx, OSC_SEG_CRC_OFF) != rd16(&rx[OSC_SEG_CRC_OFF])) {
		return SEG_BAD_CRC;
	}
	out->idx = rx[1];
	out->wseq = rd16(&rx[2]);
	out->preset = rx[4];
	out->cmd_ack = rx[5];
	out->payload = &rx[OSC_SEG_HDR];
	return SEG_OK;
}

void osc_cmd_build(uint8_t tx[OSC_SEG_LEN], uint8_t cmd, uint8_t arg, uint8_t seq)
{
	memset(tx, 0, OSC_SEG_LEN);
	tx[0] = OSC_SYNC_RX;
	tx[1] = cmd;
	tx[2] = arg;
	tx[3] = seq;
	uint16_t crc = osc_crc16(tx, 6);

	tx[6] = (uint8_t)(crc & 0xFF);
	tx[7] = (uint8_t)(crc >> 8);
}

int osc_stream_decode(const uint8_t stream[OSC_STREAM_LEN], uint16_t wseq,
		      uint8_t seg_preset, struct osc_window *w)
{
	w->wseq = wseq;
	if (stream[6] == OSC_NCH && stream[7] == 12) {
		/* Version 1: un solo preset, canales intercalados (firmware anterior) */
		w->version = 1;
		w->mask = 0x3;
		w->aligned = 1;
		w->fs[0] = w->fs[1] = rd32(&stream[0]);
		w->preset[0] = w->preset[1] = seg_preset;
		w->n = rd16(&stream[4]);
		w->bits = stream[7];
		if (w->n != OSC_WIN_SAMPLES) {
			return -1;
		}
		for (unsigned i = 0; i < OSC_WIN_SAMPLES; i++) {
			for (unsigned c = 0; c < OSC_NCH; c++) {
				w->ch[c][i] = rd16(&stream[OSC_WINHDR_V1 + 2 * (OSC_NCH * i + c)]);
			}
		}
		return 0;
	}
	if (stream[0] != 2) {
		return -1;
	}
	/* Version 2: preset y frecuencia por canal, datos por bloques */
	w->version = 2;
	w->mask = stream[1] & 0x3;
	w->bits = stream[2];
	w->aligned = (stream[3] & OSC_FLAG_ALIGNED) ? 1 : 0;
	w->n = rd16(&stream[4]);
	w->preset[0] = stream[6];
	w->preset[1] = stream[7];
	w->fs[0] = rd32(&stream[8]);
	w->fs[1] = rd32(&stream[12]);
	if (w->n != OSC_WIN_SAMPLES || w->preset[0] >= OSC_NPRESETS || w->preset[1] >= OSC_NPRESETS) {
		return -1;
	}
	for (unsigned c = 0; c < OSC_NCH; c++) {
		for (unsigned i = 0; i < OSC_WIN_SAMPLES; i++) {
			w->ch[c][i] = rd16(&stream[OSC_WINHDR_V2 + 2 * (c * OSC_WIN_SAMPLES + i)]);
		}
	}
	return 0;
}
