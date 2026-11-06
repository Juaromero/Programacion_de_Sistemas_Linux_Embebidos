/*
 * Protocolo SPI ESP32-S3 (esclavo) <-> Raspberry Pi (maestro).
 * Especificacion completa: Proyecto_2_Osciloscopio/Documentos/03_Protocolo_SPI.md
 *
 * El FIFO del esclavo SPI del ESP32-S3 en Zephyr admite 64 bytes por
 * activacion de CS, por lo que una ventana (280 bytes de "stream") viaja en
 * 5 segmentos de 64 bytes, uno por transaccion SPI.
 */
#ifndef OSC_PROTOCOL_H
#define OSC_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* ---- Geometria de la ventana (stream version 2) ----
 * [0] version=2 [1] mascara de canales [2] bits [3] flags (bit0 = sincronizados)
 * [4..5] n=64 [6] preset CH1 [7] preset CH2 [8..11] fs CH1 [12..15] fs CH2
 * [16..143] CH1: 64 x uint16   [144..271] CH2: 64 x uint16   [272..279] relleno */
#define OSC_WIN_SAMPLES   64u   /* muestras por canal */
#define OSC_NCH           2u
#define OSC_ADC_BITS      12u
#define OSC_STREAM_VER    2u
#define OSC_WINHDR_LEN    16u
#define OSC_FLAG_ALIGNED  0x01u
#define OSC_DATA_LEN      (OSC_WIN_SAMPLES * OSC_NCH * 2u)       /* 256 */

/* ---- Segmento SPI (64 bytes) ---- */
#define OSC_SEG_LEN       64u
#define OSC_SEG_HDR       6u    /* sync, idx, wseq(2), preset, cmd_ack */
#define OSC_SEG_PAYLOAD   56u
#define OSC_SEG_CRC_OFF   62u
#define OSC_NSEG          5u
#define OSC_STREAM_LEN    (OSC_SEG_PAYLOAD * OSC_NSEG)           /* 280 */

#define OSC_SYNC_TX       0xA5u /* ESP32 -> RPi */
#define OSC_SYNC_RX       0x5Au /* RPi -> ESP32 */
#define OSC_SEG_IDLE      0xFFu /* aun no hay ventana disponible */

/* ---- Comandos (RPi -> ESP32), primeros 8 bytes de la transaccion ---- */
#define OSC_CMD_NOP         0x00u
#define OSC_CMD_SET_PRESET  0x01u
#define OSC_CMD_RESYNC      0x02u
#define OSC_CMD_SET_CH_PRESET 0x03u /* arg = (canal << 4) | preset, canal 0 o 1 */
#define OSC_CMD_SET_MASK    0x04u   /* arg = mascara: bit0 CH1, bit1 CH2 */
#define OSC_CMD_LEN         8u  /* sync, cmd, arg, cmd_seq, rsv(2), crc(2) */

/* ---- Presets ---- */
#define OSC_NPRESETS      5u    /* 100, 1k, 10k, 41.7k y 83.3k Sa/s (este, un canal por rafaga) */

/* Descripcion de una ventana */
struct osc_win_info {
	uint32_t fs[OSC_NCH];     /* Sa/s por canal */
	uint8_t  preset[OSC_NCH];
	uint8_t  mask;            /* canales adquiridos */
	uint8_t  aligned;         /* 1 = CH1 y CH2 de la misma rafaga */
};

/* Slot de ventana (3 slots en anillo: acquisition escribe, SPI lee). */
struct osc_slot {
	uint8_t  stream[OSC_STREAM_LEN]; /* winhdr + datos + relleno */
	uint16_t wseq;
	uint8_t  preset;                 /* preset de CH1 (campo PRESET del segmento) */
};

uint16_t osc_crc16(const uint8_t *data, size_t len);

/* Construye el stream (version 2) de una ventana con los datos de cada canal. */
void osc_slot_fill(struct osc_slot *s, const uint16_t *d1, const uint16_t *d2,
		   const struct osc_win_info *info, uint16_t wseq);

/* Genera el segmento idx (0..4) del slot; con s == NULL genera IDLE. */
void osc_segment_build(uint8_t out[OSC_SEG_LEN], const struct osc_slot *s,
		       uint8_t idx, uint8_t cmd_ack);

/* Valida y decodifica un comando recibido. Devuelve 0 si es valido. */
int osc_cmd_parse(const uint8_t *rx, uint8_t *cmd, uint8_t *arg, uint8_t *seq);

/* Gestion de slots (anillo de 3). Devuelve indices 0..2 o -1. */
int osc_slot_get_free(void);
void osc_slot_publish(int idx);
int osc_slot_lock_latest(void);   /* marca el ultimo slot como en uso */
void osc_slot_unlock(void);
struct osc_slot *osc_slot_ptr(int idx);

#endif /* OSC_PROTOCOL_H */
