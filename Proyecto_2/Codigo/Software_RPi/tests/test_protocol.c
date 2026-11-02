/* Pruebas unitarias del protocolo (sin hardware): UT-SBC-02 (CRC) y trama. */
#include <stdio.h>
#include <string.h>
#include "protocol.h"

static int fails;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } \
			   else { printf("ok:   %s\n", msg); } } while (0)

/* Replica del constructor de segmentos del firmware (protocol.c del ESP32) */
static void esp_build(uint8_t out[64], const uint8_t stream[280], uint8_t idx,
		      uint16_t wseq, uint8_t preset, uint8_t ack)
{
	out[0] = OSC_SYNC_TX; out[1] = idx; out[2] = wseq & 0xFF; out[3] = wseq >> 8;
	out[4] = preset; out[5] = ack;
	memcpy(out + 6, stream + idx * 56, 56);
	uint16_t c = osc_crc16(out, 62);

	out[62] = c & 0xFF; out[63] = c >> 8;
}

int main(void)
{
	CHECK(osc_crc16((const uint8_t *)"123456789", 9) == 0x29B1, "CRC-16/CCITT-FALSE vector 0x29B1");

	uint8_t stream[280] = { 0 };
	uint32_t fs = 10000;

	memcpy(stream, &fs, 4);            /* little-endian en RPi/ESP32 */
	stream[4] = 64; stream[5] = 0; stream[6] = 2; stream[7] = 12;
	for (int i = 0; i < 128; i++) {
		uint16_t v = (uint16_t)(i * 31 % 4096);
		stream[8 + 2 * i] = v & 0xFF; stream[9 + 2 * i] = v >> 8;
	}
	uint8_t seg[5][64];
	struct osc_seg s;

	for (int i = 0; i < 5; i++) {
		esp_build(seg[i], stream, i, 777, 2, 9);
		CHECK(osc_seg_parse(seg[i], &s) == SEG_OK && s.idx == i && s.wseq == 777 &&
		      s.preset == 2 && s.cmd_ack == 9, "segmento valido parseado");
	}
	uint8_t bad[64];

	memcpy(bad, seg[1], 64);
	bad[20] ^= 0x01;
	CHECK(osc_seg_parse(bad, &s) == SEG_BAD_CRC, "bit corrupto -> CRC invalido (descarte)");
	memcpy(bad, seg[1], 64);
	bad[0] = 0x00;
	CHECK(osc_seg_parse(bad, &s) == SEG_BAD_SYNC, "sync incorrecto -> descarte");

	uint8_t rs[280];

	for (int i = 0; i < 5; i++) {
		osc_seg_parse(seg[i], &s);
		memcpy(rs + i * 56, s.payload, 56);
	}
	struct osc_window w;

	CHECK(osc_stream_decode(rs, 777, 2, &w) == 0 && w.fs_hz == 10000 && w.n == 64 &&
	      w.n_ch == 2 && w.bits == 12, "encabezado de ventana");
	CHECK(w.ch[0][0] == 0 && w.ch[1][0] == 31 && w.ch[0][1] == 62 && w.ch[1][63] == (127 * 31 % 4096),
	      "intercalado CH1/CH2");

	uint8_t cmd[64];

	osc_cmd_build(cmd, OSC_CMD_SET_PRESET, 3, 5);
	CHECK(cmd[0] == 0x5A && cmd[1] == 1 && cmd[2] == 3 && cmd[3] == 5 &&
	      osc_crc16(cmd, 6) == (uint16_t)(cmd[6] | cmd[7] << 8), "comando SET_PRESET con CRC");

	printf("%s (%d fallos)\n", fails ? "RESULTADO: FALLO" : "RESULTADO: OK", fails);
	return fails ? 1 : 0;
}
