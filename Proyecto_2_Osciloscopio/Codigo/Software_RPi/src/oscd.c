/*
 * oscd - servicio de adquisicion del osciloscopio (Raspberry Pi 4, SPI maestro).
 *
 *  - Lee ventanas del ESP32-S3 por /dev/spidev (5 segmentos de 64 bytes).
 *  - Valida CRC-16 y la secuencia de segmentos; descarta tramas corruptas.
 *  - Entiende el formato de ventana v1 (un preset para ambos canales) y v2
 *    (preset, frecuencia y activacion por canal).
 *  - Cada T_r (4 s .. 1 s) escribe la ultima ventana valida en un JSON
 *    (escritura atomica: archivo temporal + rename) que sirve nginx.
 *  - Mini API HTTP local (127.0.0.1) para presets, canales y refresco; nginx la
 *    publica en /api/ mediante proxy_pass.
 *  - Cola de comandos con confirmacion (CMD_ACK) y tiempo limite.
 *  - Modo --bench N: mide tiempos de transferencia y errores (pruebas IT-0x).
 *  - Modo --demo: ventanas sinteticas, sin ESP32 ni SPI.
 */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>

#include "protocol.h"
#include "spi_master.h"

#define VFS_MV 3100.0           /* fondo de escala aprox. con atenuacion 12 dB */
#define CMD_TIMEOUT_XFERS 400   /* transacciones sin confirmacion antes de descartar un comando */

static const uint32_t PRESET_FS[OSC_NPRESETS] = { 100, 1000, 10000, 41667, 83333 };

struct stats {
	uint64_t windows, lost, dup, backward, esp_resets, crc_err, sync_err, io_err, soft_resync;
	uint64_t cmd_sent, cmd_timeout;
	uint64_t seg_n, seg_sum_ns, seg_min_ns, seg_max_ns;
};

static volatile sig_atomic_t running = 1;
static struct spi_master spim;
static struct stats st = { .seg_min_ns = UINT64_MAX };
static struct osc_window latest;
static int have_latest;
static uint16_t last_wseq;
static int have_last;
static double refresh_hz = 1.0;
static int verbose;
static int demo;

/* Estado deseado (lo que pidio el usuario); se reenvia si el ESP32 se reinicia */
static int want_preset[OSC_NCH] = { -1, -1 };
static int want_mask = -1;

static void on_signal(int s) { (void)s; running = 0; }

/* ---- Cola de comandos ---- */

struct cmd { uint8_t cmd, arg; };
static struct cmd cq[16];
static unsigned cq_n;
static int cmd_active;          /* el comando del frente ya tiene numero y se esta enviando */
static unsigned cmd_tries;
static uint8_t cmd_seq;
static int cmd_seq_known;

static uint8_t next_seq(uint8_t s) { return (uint8_t)(s == 255 ? 1 : s + 1); }

static void cq_activate(void)
{
	if (cq_n && cmd_seq_known && !cmd_active) {
		cmd_seq = next_seq(cmd_seq);
		cmd_active = 1;
		cmd_tries = 0;
		st.cmd_sent++;
	}
}

static void cq_push(uint8_t cmd, uint8_t arg)
{
	/* Si ya hay un comando igual esperando (no el activo), reemplazar su argumento */
	for (unsigned i = cmd_active ? 1 : 0; i < cq_n; i++) {
		if (cq[i].cmd == cmd && (cmd != OSC_CMD_SET_CH_PRESET || (cq[i].arg >> 4) == (arg >> 4))) {
			cq[i].arg = arg;
			return;
		}
	}
	if (cq_n < sizeof(cq) / sizeof(cq[0])) {
		cq[cq_n].cmd = cmd;
		cq[cq_n].arg = arg;
		cq_n++;
	}
	cq_activate();
}

static void cq_pop(void)
{
	if (cq_n) {
		memmove(&cq[0], &cq[1], (cq_n - 1) * sizeof(cq[0]));
		cq_n--;
	}
	cmd_active = 0;
	cq_activate();
}

static void request_preset(int ch, int preset)   /* ch: 0, 1 o -1 = ambos */
{
	if (ch < 0) {
		want_preset[0] = want_preset[1] = preset;
		cq_push(OSC_CMD_SET_PRESET, (uint8_t)preset);
	} else {
		want_preset[ch] = preset;
		cq_push(OSC_CMD_SET_CH_PRESET, (uint8_t)((ch << 4) | preset));
	}
}

static void request_mask(int mask)
{
	want_mask = mask & 0x3;
	cq_push(OSC_CMD_SET_MASK, (uint8_t)want_mask);
}

static void resend_wanted(void)
{
	if (want_mask >= 0) {
		cq_push(OSC_CMD_SET_MASK, (uint8_t)want_mask);
	}
	for (int c = 0; c < (int)OSC_NCH; c++) {
		if (want_preset[c] >= 0) {
			cq_push(OSC_CMD_SET_CH_PRESET, (uint8_t)((c << 4) | want_preset[c]));
		}
	}
}

/* ---- Lectura de una ventana completa ---- */

/* Devuelve 1 = ventana nueva, 0 = sin ventana nueva (idle/duplicada), -1 = error */
static int link_read_window(struct osc_window *out)
{
	uint8_t tx[OSC_SEG_LEN], rx[OSC_SEG_LEN], stream[OSC_STREAM_LEN];
	unsigned expect = 0;
	uint16_t aw = 0;
	uint8_t apreset = 0;

	for (unsigned k = 0; k < 3 * OSC_NSEG; k++) {
		uint64_t dt = 0;

		if (cmd_active) {
			osc_cmd_build(tx, cq[0].cmd, cq[0].arg, cmd_seq);
			if (++cmd_tries > CMD_TIMEOUT_XFERS) {
				/* p. ej. firmware anterior que no conoce el comando */
				st.cmd_timeout++;
				if (verbose) {
					fprintf(stderr, "comando 0x%02x sin confirmar: descartado\n", cq[0].cmd);
				}
				cq_pop();
			}
		} else {
			osc_cmd_build(tx, OSC_CMD_NOP, 0, 0);
		}
		if (spim_xfer(&spim, tx, rx, &dt) < 0) {
			st.io_err++;
			return -1;
		}
		st.seg_n++;
		st.seg_sum_ns += dt;
		if (dt < st.seg_min_ns) st.seg_min_ns = dt;
		if (dt > st.seg_max_ns) st.seg_max_ns = dt;

		struct osc_seg s;
		int r = osc_seg_parse(rx, &s);

		if (r != SEG_OK) {
			if (r == SEG_BAD_SYNC) st.sync_err++; else st.crc_err++;
			expect = 0;
			continue;
		}
		if (!cmd_seq_known) {          /* alinear cmd_seq con el del esclavo */
			cmd_seq = s.cmd_ack;
			cmd_seq_known = 1;
			cq_activate();
		} else if (cmd_active && s.cmd_ack == cmd_seq) {
			cq_pop();                  /* confirmado por el esclavo */
		}
		if (s.idx == OSC_SEG_IDLE) {
			return 0;
		}
		if (s.idx == 0) {
			aw = s.wseq;
			apreset = s.preset;
			memcpy(stream, s.payload, OSC_SEG_PAYLOAD);
			expect = 1;
		} else if (expect && s.idx == expect && s.wseq == aw && s.idx < OSC_NSEG) {
			memcpy(stream + s.idx * OSC_SEG_PAYLOAD, s.payload, OSC_SEG_PAYLOAD);
			if (++expect == OSC_NSEG) {
				/* Si nadie leia el bus, el esclavo dejo congelada una ventana
				 * antigua: la primera tras arrancar se descarta. */
				static int warmed;

				if (!warmed) {
					warmed = 1;
					return 0;
				}
				if (have_last && aw == last_wseq) {
					st.dup++;
					return 0;
				}
				if (osc_stream_decode(stream, aw, apreset, out) < 0) {
					st.crc_err++;
					return -1;
				}
				if (have_last) {
					int16_t d = (int16_t)(aw - last_wseq);

					if (d < 0 && d > -16) {
						st.backward++;   /* un poco mas vieja: no publicar */
						return 0;
					}
					if (d < 0) {
						/* salto grande hacia atras: el ESP32 se reinicio */
						st.esp_resets++;
						fprintf(stderr, "ESP32 reiniciado (WSEQ %u -> %u)\n", last_wseq, aw);
						resend_wanted();
					} else {
						st.lost += (uint64_t)(d - 1);
					}
				}
				last_wseq = aw;
				have_last = 1;
				st.windows++;
				return 1;
			}
		} else {
			st.soft_resync++;
			expect = 0;
		}
	}
	return -1;
}

/* ---- Modo demostracion: ventanas sinteticas, sin ESP32 ni SPI ---- */

static int demo_window(struct osc_window *w)
{
	static uint16_t seq;
	static uint8_t preset[OSC_NCH], mask = 0x3;
	static uint64_t next_ns;

	while (cq_n) {                  /* "ejecuta" los comandos como el ESP32 */
		if (cq[0].cmd == OSC_CMD_SET_PRESET && cq[0].arg < OSC_NPRESETS) {
			preset[0] = preset[1] = cq[0].arg;
		} else if (cq[0].cmd == OSC_CMD_SET_CH_PRESET && (cq[0].arg & 0xF) < OSC_NPRESETS) {
			preset[(cq[0].arg >> 4) & 1] = cq[0].arg & 0xF;
		} else if (cq[0].cmd == OSC_CMD_SET_MASK) {
			mask = cq[0].arg & 0x3;
		}
		memmove(&cq[0], &cq[1], (cq_n - 1) * sizeof(cq[0]));
		cq_n--;
	}
	cmd_active = 0;

	uint64_t now = now_ns();

	if (now < next_ns) {
		return 0;
	}
	next_ns = now + 50000000ull;    /* 20 ventanas/s */
	w->wseq = seq++;
	w->version = 2;
	w->mask = mask;
	w->aligned = (mask == 0x3 && preset[0] == preset[1] && preset[0] != 4);
	w->n = OSC_WIN_SAMPLES;
	w->bits = 12;
	for (unsigned c = 0; c < OSC_NCH; c++) {
		w->preset[c] = preset[c];
		w->fs[c] = PRESET_FS[preset[c]];
		for (unsigned i = 0; i < OSC_WIN_SAMPLES; i++) {
			double ph = 2.0 * M_PI * i / (c == 0 ? 16.0 : 32.0);
			w->ch[c][i] = (mask & (1u << c)) ?
				(uint16_t)(2048 + 1700 * sin(ph + seq * 0.05)) : 0;
		}
	}
	last_wseq = w->wseq;
	have_last = 1;
	st.windows++;
	return 1;
}

/* ---- JSON ---- */

static int build_json(char *buf, size_t sz)
{
	const struct osc_window *w = &latest;
	int n = snprintf(buf, sz,
		"{\"seq\":%u,\"version\":%u,\"mask\":%u,\"aligned\":%u,\"n\":%u,\"bits\":%u,"
		"\"vfs_mv\":%.0f,\"refresh_hz\":%.2f,\"ts\":%ld,\"fs\":%u,\"preset\":%u,\"channels\":[",
		w->wseq, w->version, w->mask, w->aligned, w->n, w->bits, VFS_MV, refresh_hz,
		(long)time(NULL), w->fs[0], w->preset[0]);
	for (unsigned c = 0; c < OSC_NCH; c++) {
		n += snprintf(buf + n, sz - n, "%s{\"id\":%u,\"on\":%s,\"fs\":%u,\"preset\":%u}",
			      c ? "," : "", c + 1, (w->mask & (1u << c)) ? "true" : "false",
			      w->fs[c], w->preset[c]);
	}
	n += snprintf(buf + n, sz - n, "],\"ch1\":[");
	for (unsigned c = 0; c < OSC_NCH; c++) {
		for (unsigned i = 0; i < OSC_WIN_SAMPLES; i++) {
			n += snprintf(buf + n, sz - n, "%s%u", i ? "," : "", w->ch[c][i]);
		}
		n += snprintf(buf + n, sz - n, c == 0 ? "],\"ch2\":[" : "]");
	}
	double avg_seg = st.seg_n ? (double)st.seg_sum_ns / st.seg_n / 1000.0 : 0;

	n += snprintf(buf + n, sz - n,
		",\"stats\":{\"windows\":%llu,\"lost\":%llu,\"dup\":%llu,\"backward\":%llu,\"esp_resets\":%llu,"
		"\"crc_err\":%llu,\"sync_err\":%llu,\"io_err\":%llu,\"soft_resync\":%llu,"
		"\"cmd_sent\":%llu,\"cmd_timeout\":%llu,\"cmd_pending\":%u,\"spi_hz\":%u,\"gap_us\":%u,"
		"\"seg_avg_us\":%.1f,\"want_preset\":[%d,%d],\"want_mask\":%d}}\n",
		(unsigned long long)st.windows, (unsigned long long)st.lost,
		(unsigned long long)st.dup, (unsigned long long)st.backward,
		(unsigned long long)st.esp_resets,
		(unsigned long long)st.crc_err, (unsigned long long)st.sync_err,
		(unsigned long long)st.io_err, (unsigned long long)st.soft_resync,
		(unsigned long long)st.cmd_sent, (unsigned long long)st.cmd_timeout, cq_n,
		spim.speed_hz, spim.gap_us, avg_seg, want_preset[0], want_preset[1], want_mask);
	return n;
}

static void write_json(const char *path)
{
	static char buf[4096];
	char tmp[256];

	build_json(buf, sizeof(buf));
	snprintf(tmp, sizeof(tmp), "%s.tmp", path);
	FILE *f = fopen(tmp, "w");

	if (!f) {
		perror(tmp);
		return;
	}
	fputs(buf, f);
	fflush(f);
	fclose(f);
	chmod(tmp, 0644);
	if (rename(tmp, path) < 0) {   /* atomico: nginx nunca ve un archivo parcial */
		perror("rename");
	}
}

/* ---- Mini API HTTP (solo localhost) ---- */

static int http_open(int port)
{
	int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	int one = 1;
	struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(port),
				 .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };

	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	if (bind(fd, (struct sockaddr *)&a, sizeof(a)) < 0 || listen(fd, 4) < 0) {
		perror("http bind/listen");
		close(fd);
		return -1;
	}
	return fd;
}

static void http_reply(int c, int code, const char *body)
{
	char h[640];
	int n = snprintf(h, sizeof(h),
		"HTTP/1.1 %d %s\r\nContent-Type: application/json\r\nCache-Control: no-store\r\n"
		"Content-Length: %zu\r\nConnection: close\r\n\r\n%s",
		code, code == 200 ? "OK" : "Bad Request", strlen(body), body);

	if (write(c, h, n) < 0) { /* ignorar */ }
}

/* Valor numerico del parametro "name=" en la primera linea de la peticion, o def */
static double qparam(const char *req, const char *name, double def)
{
	const char *eol = strstr(req, "\r\n");
	char key[16];

	snprintf(key, sizeof(key), "%s=", name);
	for (const char *p = req; (p = strstr(p, key)) != NULL; p++) {
		if (eol && p > eol) {
			break;
		}
		if (p > req && (p[-1] == '?' || p[-1] == '&')) {
			return atof(p + strlen(key));
		}
	}
	return def;
}

static void http_serve(int lfd)
{
	int c = accept4(lfd, NULL, NULL, SOCK_NONBLOCK);

	if (c < 0) {
		return;
	}
	struct pollfd p = { .fd = c, .events = POLLIN };
	char req[1024] = { 0 }, body[320];

	if (poll(&p, 1, 50) > 0) {
		if (read(c, req, sizeof(req) - 1) < 0) {
			req[0] = '\0';
		}
	}
	if (strstr(req, "/api/preset")) {
		int id = (int)qparam(req, "id", -1);
		int ch = (int)qparam(req, "ch", 0);     /* 1, 2 o ausente = ambos */

		if (id >= 0 && id < (int)OSC_NPRESETS && ch >= 0 && ch <= 2) {
			request_preset(ch - 1, id);
			snprintf(body, sizeof(body), "{\"ok\":true,\"ch\":%d,\"preset\":%d,\"fs\":%u}",
				 ch, id, PRESET_FS[id]);
			http_reply(c, 200, body);
		} else {
			http_reply(c, 400, "{\"ok\":false,\"error\":\"id 0..4, ch 1..2\"}");
		}
	} else if (strstr(req, "/api/channel")) {
		int ch = (int)qparam(req, "ch", 0), on = (int)qparam(req, "on", -1);

		if ((ch == 1 || ch == 2) && (on == 0 || on == 1)) {
			int base = want_mask >= 0 ? want_mask : (have_latest ? latest.mask : 0x3);
			int bit = 1 << (ch - 1);

			request_mask(on ? (base | bit) : (base & ~bit));
			snprintf(body, sizeof(body), "{\"ok\":true,\"mask\":%d}", want_mask);
			http_reply(c, 200, body);
		} else {
			http_reply(c, 400, "{\"ok\":false,\"error\":\"ch 1..2, on 0..1\"}");
		}
	} else if (strstr(req, "/api/refresh")) {
		double hz = qparam(req, "hz", 0);

		if (hz >= 0.25 && hz <= 1.0) {
			refresh_hz = hz;
			snprintf(body, sizeof(body), "{\"ok\":true,\"refresh_hz\":%.2f}", hz);
			http_reply(c, 200, body);
		} else {
			http_reply(c, 400, "{\"ok\":false,\"error\":\"hz 0.25..1\"}");
		}
	} else if (strstr(req, "/api/status")) {
		snprintf(body, sizeof(body),
			 "{\"want_preset\":[%d,%d],\"want_mask\":%d,\"mask\":%u,\"fs\":[%u,%u],"
			 "\"refresh_hz\":%.2f,\"windows\":%llu,\"cmd_pending\":%u}",
			 want_preset[0], want_preset[1], want_mask, latest.mask, latest.fs[0],
			 latest.fs[1], refresh_hz, (unsigned long long)st.windows, cq_n);
		http_reply(c, 200, body);
	} else {
		http_reply(c, 400, "{\"ok\":false}");
	}
	close(c);
}

/* ---- Banco de pruebas ---- */

static unsigned bench_secs = 30;   /* tope de duracion (sin ESP32 no hay ventanas) */

static void bench(unsigned nwin)
{
	uint64_t t0 = now_ns();
	struct osc_window w;
	unsigned got = 0, win_to_change = 0;
	uint64_t t_change = 0;

	while (running && got < nwin && (now_ns() - t0) < bench_secs * 1000000000ull) {
		int r = link_read_window(&w);

		if (r == 1) {
			got++;
			latest = w;
			have_latest = 1;
			if (want_preset[0] >= 0 && !t_change && w.preset[0] == want_preset[0]) {
				t_change = now_ns() - t0;   /* primera ventana con el preset pedido */
				win_to_change = got;
				t0 = now_ns();              /* la tasa se mide ya con el preset nuevo */
				got = 0;
			}
		} else {
			usleep(500);
		}
	}
	double el = (now_ns() - t0) / 1e9;
	double avg = st.seg_n ? (double)st.seg_sum_ns / st.seg_n / 1000.0 : 0;
	double bits_t = OSC_SEG_LEN * 8.0 / (spim.speed_hz);

	if (want_preset[0] >= 0) {
		if (t_change) {
			printf("BENCH cambio a preset %d: %.1f ms, %u ventana(s) recibida(s) hasta ver el preset nuevo\n",
			       want_preset[0], t_change / 1e6, win_to_change);
		} else {
			printf("BENCH cambio a preset %d: NO observado\n", want_preset[0]);
		}
	}
	printf("BENCH ventanas=%u en %.3f s (%.1f ventanas/s)\n", got, el, got / el);
	printf("BENCH transacciones=%llu  ioctl_us avg=%.1f min=%.1f max=%.1f  (t_reloj_teorico=%.1f us)\n",
	       (unsigned long long)st.seg_n, avg, st.seg_min_ns / 1000.0, st.seg_max_ns / 1000.0,
	       bits_t * 1e6);
	printf("BENCH lost=%llu dup=%llu backward=%llu crc_err=%llu sync_err=%llu io_err=%llu soft_resync=%llu cmd_timeout=%llu\n",
	       (unsigned long long)st.lost, (unsigned long long)st.dup, (unsigned long long)st.backward,
	       (unsigned long long)st.crc_err, (unsigned long long)st.sync_err,
	       (unsigned long long)st.io_err, (unsigned long long)st.soft_resync,
	       (unsigned long long)st.cmd_timeout);
	if (have_latest) {
		printf("BENCH ultima ventana: seq=%u formato=v%u mascara=%u %s CH1=%u Sa/s CH2=%u Sa/s\n",
		       latest.wseq, latest.version, latest.mask,
		       latest.aligned ? "sincronizados" : "independientes", latest.fs[0], latest.fs[1]);
		for (int c = 0; c < 2; c++) {
			printf("BENCH CH%d:", c + 1);
			for (unsigned i = 0; i < OSC_WIN_SAMPLES; i++) printf(" %u", latest.ch[c][i]);
			printf("\n");
		}
	}
}

int main(int argc, char **argv)
{
	const char *dev = "/dev/spidev0.0", *out = "/run/osciloscopio/window.json";
	uint32_t speed = 8000000, gap = 150;
	int port = 8081, bench_n = 0, rt = 0, set_preset = -1, set_mask = -1, opt;
	static const struct option lo[] = {
		{ "dev", 1, 0, 'd' }, { "speed", 1, 0, 's' }, { "gap-us", 1, 0, 'g' },
		{ "out", 1, 0, 'o' }, { "port", 1, 0, 'p' }, { "refresh", 1, 0, 'r' },
		{ "bench", 1, 0, 'b' }, { "bench-secs", 1, 0, 'T' }, { "preset", 1, 0, 'P' },
		{ "mask", 1, 0, 'M' }, { "rt", 0, 0, 'R' }, { "verbose", 0, 0, 'v' },
		{ "demo", 0, 0, 'D' }, { 0, 0, 0, 0 } };

	while ((opt = getopt_long(argc, argv, "d:s:g:o:p:r:b:T:P:M:RvD", lo, NULL)) != -1) {
		switch (opt) {
		case 'd': dev = optarg; break;
		case 's': speed = (uint32_t)atoi(optarg); break;
		case 'g': gap = (uint32_t)atoi(optarg); break;
		case 'o': out = optarg; break;
		case 'p': port = atoi(optarg); break;
		case 'r': refresh_hz = atof(optarg); break;
		case 'b': bench_n = atoi(optarg); break;
		case 'T': bench_secs = (unsigned)atoi(optarg); break;
		case 'P': set_preset = atoi(optarg); break;
		case 'M': set_mask = atoi(optarg); break;
		case 'R': rt = 1; break;
		case 'v': verbose = 1; break;
		case 'D': demo = 1; break;
		default:
			fprintf(stderr, "uso: %s [--dev D] [--speed Hz] [--gap-us N] [--out F] [--port P]\n"
				"          [--refresh 0.25..1] [--preset 0..4] [--mask 0..3] [--bench N]\n"
				"          [--bench-secs S] [--demo] [--rt] [-v]\n", argv[0]);
			return 2;
		}
	}
	if (refresh_hz < 0.25 || refresh_hz > 1.0) {
		fprintf(stderr, "refresh fuera de rango (0.25..1 Hz)\n");
		return 2;
	}
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);
	prctl(PR_SET_TIMERSLACK, 1UL);
	if (rt) {
		struct sched_param sp = { .sched_priority = 20 };

		if (sched_setscheduler(0, SCHED_FIFO, &sp) < 0) perror("SCHED_FIFO");
	}
	if (demo) {
		spim.fd = -1;
		spim.speed_hz = speed;
		spim.gap_us = gap;
		cmd_seq_known = 1;
		fprintf(stderr, "oscd: MODO DEMO (ventanas sinteticas, sin SPI)\n");
	} else if (spim_open(&spim, dev, speed, gap) < 0) {
		return 1;
	}
	if (set_mask >= 0 && set_mask <= 3) {
		request_mask(set_mask);
	}
	if (set_preset >= 0 && set_preset < (int)OSC_NPRESETS) {
		request_preset(-1, set_preset);
	}
	if (bench_n > 0) {
		bench((unsigned)bench_n);
		spim_close(&spim);
		return 0;
	}
	int lfd = http_open(port);
	uint64_t next_write = now_ns();

	fprintf(stderr, "oscd: %s a %u Hz, gap %u us, salida %s, API 127.0.0.1:%d\n", dev, speed,
		gap, out, port);
	while (running) {
		struct osc_window w;
		int r = demo ? demo_window(&w) : link_read_window(&w);

		if (r == 1) {
			latest = w;
			have_latest = 1;
		}
		uint64_t now = now_ns();

		if (have_latest && now >= next_write) {
			write_json(out);
			next_write = now + (uint64_t)(1e9 / refresh_hz);
			if (verbose) {
				fprintf(stderr, "seq=%u v%u mask=%u fs=%u/%u win=%llu lost=%llu crc=%llu\n",
					latest.wseq, latest.version, latest.mask, latest.fs[0], latest.fs[1],
					(unsigned long long)st.windows, (unsigned long long)st.lost,
					(unsigned long long)st.crc_err);
			}
		}
		if (lfd >= 0) {
			http_serve(lfd);
		}
		if (r != 1) {
			usleep(1000);   /* sin ventana nueva: no saturar el bus ni la CPU */
		}
	}
	spim_close(&spim);
	return 0;
}
