/*
 * Protocolo SPI RPi (maestro) <-> ESP32-S3 (esclavo).
 * Debe coincidir con Firmware_ESP32S3/src/protocol.h.
 * Especificacion: Proyecto_2_Osciloscopio/Documentos/03_Protocolo_SPI.md
 */
#ifndef OSC_PROTOCOL_H
#define OSC_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#define OSC_WIN_SAMPLES   64u
#define OSC_NCH           2u
#define OSC_DATA_LEN      (OSC_WIN_SAMPLES * OSC_NCH * 2u)   /* 256 */

#define OSC_SEG_LEN       64u
#define OSC_SEG_HDR       6u
#define OSC_SEG_PAYLOAD   56u
#define OSC_SEG_CRC_OFF   62u
#define OSC_NSEG          5u
#define OSC_STREAM_LEN    (OSC_SEG_PAYLOAD * OSC_NSEG)       /* 280 */

/* Stream version 1 (firmware anterior): fs(u32) n(u16) n_ch(u8)=2 bits(u8)=12,
 *   datos intercalados CH1,CH2.
 * Stream version 2: [0]=2 [1]=mascara [2]=bits [3]=flags [4..5]=n [6]=preset CH1
 *   [7]=preset CH2 [8..11]=fs CH1 [12..15]=fs CH2 [16..143]=CH1 [144..271]=CH2 */
#define OSC_WINHDR_V1     8u
#define OSC_WINHDR_V2     16u
#define OSC_FLAG_ALIGNED  0x01u

#define OSC_SYNC_TX       0xA5u   /* ESP32 -> RPi */
#define OSC_SYNC_RX       0x5Au   /* RPi -> ESP32 */
#define OSC_SEG_IDLE      0xFFu

#define OSC_CMD_NOP           0x00u
#define OSC_CMD_SET_PRESET    0x01u   /* arg = preset, ambos canales */
#define OSC_CMD_RESYNC        0x02u
#define OSC_CMD_SET_CH_PRESET 0x03u   /* arg = (canal << 4) | preset */
#define OSC_CMD_SET_MASK      0x04u   /* arg = bit0 CH1, bit1 CH2 */

#define OSC_NPRESETS      5u

/* CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF). "123456789" -> 0x29B1 */
uint16_t osc_crc16(const uint8_t *data, size_t len);

/* Segmento recibido, ya validado */
struct osc_seg {
	uint8_t  idx;        /* 0..4 o OSC_SEG_IDLE */
	uint16_t wseq;
	uint8_t  preset;
	uint8_t  cmd_ack;
	const uint8_t *payload; /* 56 bytes */
};

enum osc_seg_status { SEG_OK = 0, SEG_BAD_SYNC = -1, SEG_BAD_CRC = -2 };

int osc_seg_parse(const uint8_t rx[OSC_SEG_LEN], struct osc_seg *out);
void osc_cmd_build(uint8_t tx[OSC_SEG_LEN], uint8_t cmd, uint8_t arg, uint8_t seq);

/* Ventana completa decodificada */
struct osc_window {
	uint16_t wseq;
	uint8_t  version;          /* 1 o 2 */
	uint8_t  mask;             /* canales adquiridos */
	uint8_t  aligned;          /* 1 = CH1 y CH2 de la misma rafaga */
	uint8_t  preset[OSC_NCH];
	uint32_t fs[OSC_NCH];
	uint16_t n;
	uint8_t  bits;
	uint16_t ch[OSC_NCH][OSC_WIN_SAMPLES];
};

int osc_stream_decode(const uint8_t stream[OSC_STREAM_LEN], uint16_t wseq,
		      uint8_t seg_preset, struct osc_window *w);

#endif /* OSC_PROTOCOL_H */
