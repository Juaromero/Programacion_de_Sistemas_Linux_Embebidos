/*
 * Protocolo SPI ESP32-S3 (esclavo) <-> Raspberry Pi (maestro).
 * Especificacion completa: Proyecto_2/Documentos/Protocolo_SPI.md
 *
 * El FIFO del esclavo SPI del ESP32-S3 en Zephyr admite 64 bytes por
 * activacion de CS, por lo que una ventana (280 bytes de "stream") viaja en
 * 5 segmentos de 64 bytes, uno por transaccion SPI.
 */
#ifndef OSC_PROTOCOL_H
#define OSC_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* ---- Geometria de la ventana ---- */
#define OSC_WIN_SAMPLES   64u   /* muestras por canal */
#define OSC_NCH           2u
#define OSC_ADC_BITS      12u
#define OSC_WINHDR_LEN    8u    /* fs_hz(u32) n(u16) n_ch(u8) bits(u8) */
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
#define OSC_CMD_LEN         8u  /* sync, cmd, arg, cmd_seq, rsv(2), crc(2) */

/* ---- Presets ---- */
#define OSC_NPRESETS      4u

/* Slot de ventana (3 slots en anillo: acquisition escribe, SPI lee). */
struct osc_slot {
	uint8_t  stream[OSC_STREAM_LEN]; /* winhdr + datos + relleno */
	uint16_t wseq;
	uint8_t  preset;
};

uint16_t osc_crc16(const uint8_t *data, size_t len);

/* Construye el stream de una ventana a partir de muestras intercaladas
 * [CH1,CH2,CH1,CH2,...] (64 pares, uint16 little-endian en memoria). */
void osc_slot_fill(struct osc_slot *s, const uint16_t *pairs, uint32_t fs_hz,
		   uint16_t wseq, uint8_t preset);

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
