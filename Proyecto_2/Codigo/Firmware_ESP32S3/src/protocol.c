/* Implementacion del protocolo SPI y del anillo de slots. */
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>

#include "protocol.h"

#define NSLOTS 3

static struct osc_slot slots[NSLOTS];
static atomic_t latest = ATOMIC_INIT(-1);
static atomic_t busy = ATOMIC_INIT(-1);

/* CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, sin reflejo, xorout 0.
 * Vector de prueba: "123456789" -> 0x29B1. */
uint16_t osc_crc16(const uint8_t *data, size_t len)
{
	return crc16_itu_t(0xFFFF, data, len);
}

void osc_slot_fill(struct osc_slot *s, const uint16_t *pairs, uint32_t fs_hz,
		   uint16_t wseq, uint8_t preset)
{
	memset(s->stream, 0, sizeof(s->stream));
	sys_put_le32(fs_hz, &s->stream[0]);
	sys_put_le16(OSC_WIN_SAMPLES, &s->stream[4]);
	s->stream[6] = OSC_NCH;
	s->stream[7] = OSC_ADC_BITS;
	for (uint32_t i = 0; i < OSC_WIN_SAMPLES * OSC_NCH; i++) {
		sys_put_le16(pairs[i], &s->stream[OSC_WINHDR_LEN + 2 * i]);
	}
	s->wseq = wseq;
	s->preset = preset;
}

void osc_segment_build(uint8_t out[OSC_SEG_LEN], const struct osc_slot *s,
		       uint8_t idx, uint8_t cmd_ack)
{
	out[0] = OSC_SYNC_TX;
	out[5] = cmd_ack;
	if (s == NULL) {
		memset(out + 1, 0, OSC_SEG_LEN - 1);
		out[1] = OSC_SEG_IDLE;
		out[5] = cmd_ack;
	} else {
		out[1] = idx;
		sys_put_le16(s->wseq, &out[2]);
		out[4] = s->preset;
		memcpy(&out[OSC_SEG_HDR], &s->stream[idx * OSC_SEG_PAYLOAD],
		       OSC_SEG_PAYLOAD);
	}
	sys_put_le16(osc_crc16(out, OSC_SEG_CRC_OFF), &out[OSC_SEG_CRC_OFF]);
}

int osc_cmd_parse(const uint8_t *rx, uint8_t *cmd, uint8_t *arg, uint8_t *seq)
{
	if (rx[0] != OSC_SYNC_RX) {
		return -1;
	}
	if (osc_crc16(rx, 6) != sys_get_le16(&rx[6])) {
		return -2;
	}
	*cmd = rx[1];
	*arg = rx[2];
	*seq = rx[3];
	return 0;
}

/* ---- Anillo de 3 slots: nunca hay conflicto entre productor y consumidor ---- */

int osc_slot_get_free(void)
{
	int l = (int)atomic_get(&latest);
	int b = (int)atomic_get(&busy);

	for (int i = 0; i < NSLOTS; i++) {
		if (i != l && i != b) {
			return i;
		}
	}
	return -1; /* inalcanzable con 3 slots */
}

void osc_slot_publish(int idx)
{
	atomic_set(&latest, idx);
}

int osc_slot_lock_latest(void)
{
	int l;

	do {
		l = (int)atomic_get(&latest);
		if (l < 0) {
			return -1;
		}
		atomic_set(&busy, l);
	} while (l != (int)atomic_get(&latest));
	return l;
}

void osc_slot_unlock(void)
{
	atomic_set(&busy, -1);
}

struct osc_slot *osc_slot_ptr(int idx)
{
	return &slots[idx];
}
