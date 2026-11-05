/*
 * oscd - servicio de adquisicion del osciloscopio (Raspberry Pi 4, SPI maestro).
 *
 *  - Lee ventanas del ESP32-S3 por /dev/spidev (5 segmentos de 64 bytes).
 *  - Valida CRC-16 y la secuencia de segmentos; descarta tramas corruptas.
 *  - Cada T_r (4 s .. 1 s) escribe la ultima ventana valida en un JSON
 *    (escritura atomica: archivo temporal + rename) que sirve nginx.
 *  - Mini API HTTP local (127.0.0.1) para cambiar preset y refresco; nginx la
 *    publica en /api/ mediante proxy_pass.
 *  - Modo --bench N: mide tiempos de transferencia y errores (pruebas IT-0x).
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

#define VFS_MV 3100.0  /* fondo de escala aprox. con atenuacion 12 dB */

struct stats {
	uint64_t windows, lost, dup, backward, esp_resets, crc_err, sync_err, io_err, soft_resync, cycles;
	uint64_t seg_n, seg_sum_ns, seg_min_ns, seg_max_ns;
	uint64_t cyc_sum_ns, cyc_max_ns;
};

static volatile sig_atomic_t running = 1;
static struct spi_master spim;
static struct stats st = { .seg_min_ns = UINT64_MAX };
static struct osc_window latest;
static int have_latest;
static uint16_t last_wseq;
static int have_last;

/* Estado de comandos */
static uint8_t cmd_pending, cmd_arg, cmd_seq;
static int cmd_seq_known;
static int preset_req = -1;
static uint8_t preset_seen;
static double refresh_hz = 1.0;
static int verbose;

static void on_signal(int s) { (void)s; running = 0; }

static uint8_t next_seq(uint8_t s) { return (uint8_t)(s == 255 ? 1 : s + 1); }

/* El numero del comando se asigna solo cuando ya se conoce el CMD_ACK del
 * esclavo; si se encola antes, se numera al alinearse (ver link_read_window). */
static void queue_command(uint8_t cmd, uint8_t arg)
{
	cmd_pending = cmd;
	cmd_arg = arg;
	if (cmd_seq_known) {
		cmd_seq = next_seq(cmd_seq);
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

		if (cmd_pending && cmd_seq_known) {
			osc_cmd_build(tx, cmd_pending, cmd_arg, cmd_seq);
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
			if (cmd_pending) {
				cmd_seq = next_seq(cmd_seq);   /* numero nuevo: el esclavo lo ejecutara */
			}
		} else if (cmd_pending && s.cmd_ack == cmd_seq) {
			cmd_pending = 0;               /* confirmado por el esclavo */
		}
		preset_seen = s.preset;
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
						/* ventana un poco mas vieja que la ultima: no publicar */
						st.backward++;
						if (verbose) {
							fprintf(stderr, "ventana hacia atras: ultima=%u recibida=%u\n",
								last_wseq, aw);
						}
						return 0;
					}
					if (d < 0) {
						/* salto grande hacia atras: el ESP32 se reinicio
						 * (WSEQ volvio a 0). Aceptar y reenviar el preset. */
						st.esp_resets++;
						fprintf(stderr, "ESP32 reiniciado (WSEQ %u -> %u)\n", last_wseq, aw);
						if (preset_req >= 0) {
							queue_command(OSC_CMD_SET_PRESET, (uint8_t)preset_req);
						}
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

static int demo;

static int demo_window(struct osc_window *w)
{
	static const uint32_t fs[OSC_NPRESETS] = { 100, 1000, 10000, 41667 };
	static uint16_t seq;
	static uint8_t preset;
	static uint64_t next_ns;

	if (cmd_pending == OSC_CMD_SET_PRESET && cmd_arg < OSC_NPRESETS) {
		preset = cmd_arg;       /* "ejecuta" el comando como lo haria el ESP32 */
		cmd_pending = 0;
	}
	/* Una ventana nueva por periodo de ventana (maximo 1 cada 20 ms) */
	uint64_t tw_ns = (uint64_t)(OSC_WIN_SAMPLES * 1e9 / fs[preset]);

	if (tw_ns < 20000000ull) tw_ns = 20000000ull;
	uint64_t now = now_ns();

	if (now < next_ns) {
		return 0;
	}
	next_ns = now + tw_ns;
	w->wseq = seq++;
	w->preset = preset;
	w->fs_hz = fs[preset];
	w->n = OSC_WIN_SAMPLES;
	w->n_ch = OSC_NCH;
	w->bits = 12;
	for (unsigned i = 0; i < OSC_WIN_SAMPLES; i++) {
		double ph = 2.0 * M_PI * i / 16.0;               /* 4 periodos por ventana */
		w->ch[0][i] = (uint16_t)(2048 + 1700 * sin(ph + seq * 0.05));
		w->ch[1][i] = (uint16_t)(2048 + 1700 * sin(ph / 2.0));  /* 2 periodos */
	}
	preset_seen = preset;
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
		"{\"seq\":%u,\"preset\":%u,\"fs\":%u,\"n\":%u,\"bits\":%u,\"vfs_mv\":%.0f,"
		"\"refresh_hz\":%.2f,\"ts\":%ld,\"ch1\":[",
		w->wseq, w->preset, w->fs_hz, w->n, w->bits, VFS_MV, refresh_hz, (long)time(NULL));
	for (int c = 0; c < 2; c++) {
		for (unsigned i = 0; i < OSC_WIN_SAMPLES; i++) {
			n += snprintf(buf + n, sz - n, "%s%u", i ? "," : "", w->ch[c][i]);
		}
		n += snprintf(buf + n, sz - n, c == 0 ? "],\"ch2\":[" : "]");
	}
	double avg_seg = st.seg_n ? (double)st.seg_sum_ns / st.seg_n / 1000.0 : 0;

	n += snprintf(buf + n, sz - n,
		",\"stats\":{\"windows\":%llu,\"lost\":%llu,\"dup\":%llu,\"backward\":%llu,\"esp_resets\":%llu,\"crc_err\":%llu,"
		"\"sync_err\":%llu,\"io_err\":%llu,\"soft_resync\":%llu,\"spi_hz\":%u,"
		"\"gap_us\":%u,\"seg_avg_us\":%.1f,\"preset_req\":%d}}\n",
		(unsigned long long)st.windows, (unsigned long long)st.lost,
		(unsigned long long)st.dup, (unsigned long long)st.backward,
		(unsigned long long)st.esp_resets,
		(unsigned long long)st.crc_err, (unsigned long long)st.sync_err, (unsigned long long)st.io_err,
		(unsigned long long)st.soft_resync, spim.speed_hz, spim.gap_us, avg_seg,
		preset_req);
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
	char h[512];
	int n = snprintf(h, sizeof(h),
		"HTTP/1.1 %d %s\r\nContent-Type: application/json\r\nCache-Control: no-store\r\n"
		"Content-Length: %zu\r\nConnection: close\r\n\r\n%s",
		code, code == 200 ? "OK" : "Bad Request", strlen(body), body);

	if (write(c, h, n) < 0) { /* ignorar */ }
}

static void http_serve(int lfd)
{
	int c = accept4(lfd, NULL, NULL, SOCK_NONBLOCK);

	if (c < 0) {
		return;
	}
	struct pollfd p = { .fd = c, .events = POLLIN };
	char req[1024] = { 0 }, body[256];

	if (poll(&p, 1, 50) > 0) {
		if (read(c, req, sizeof(req) - 1) < 0) {
			req[0] = '\0';
		}
	}
	if (strstr(req, "/api/preset")) {
		char *q = strstr(req, "id=");
		int id = q ? atoi(q + 3) : -1;

		if (id >= 0 && id < (int)OSC_NPRESETS) {
			preset_req = id;
			queue_command(OSC_CMD_SET_PRESET, (uint8_t)id);
			snprintf(body, sizeof(body), "{\"ok\":true,\"preset_req\":%d}", id);
			http_reply(c, 200, body);
		} else {
			http_reply(c, 400, "{\"ok\":false,\"error\":\"id 0..3\"}");
		}
	} else if (strstr(req, "/api/refresh")) {
		char *q = strstr(req, "hz=");
		double hz = q ? atof(q + 3) : 0;

		if (hz >= 0.25 && hz <= 1.0) {
			refresh_hz = hz;
			snprintf(body, sizeof(body), "{\"ok\":true,\"refresh_hz\":%.2f}", hz);
			http_reply(c, 200, body);
		} else {
			http_reply(c, 400, "{\"ok\":false,\"error\":\"hz 0.25..1\"}");
		}
	} else if (strstr(req, "/api/status")) {
		snprintf(body, sizeof(body),
			 "{\"preset_req\":%d,\"preset_active\":%u,\"refresh_hz\":%.2f,\"windows\":%llu}",
			 preset_req, preset_seen, refresh_hz, (unsigned long long)st.windows);
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
			if (preset_req >= 0 && !t_change && w.preset == preset_req) {
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

	if (preset_req >= 0) {
		if (t_change) {
			printf("BENCH cambio a preset %d: %.1f ms, %u ventana(s) recibida(s) hasta ver el preset nuevo\n",
			       preset_req, t_change / 1e6, win_to_change);
		} else {
			printf("BENCH cambio a preset %d: NO observado\n", preset_req);
		}
	}
	printf("BENCH ventanas=%u en %.3f s (%.1f ventanas/s)\n", got, el, got / el);
	printf("BENCH transacciones=%llu  ioctl_us avg=%.1f min=%.1f max=%.1f  (t_reloj_teorico=%.1f us)\n",
	       (unsigned long long)st.seg_n, avg, st.seg_min_ns / 1000.0, st.seg_max_ns / 1000.0,
	       bits_t * 1e6);
	printf("BENCH lost=%llu dup=%llu backward=%llu crc_err=%llu sync_err=%llu io_err=%llu soft_resync=%llu\n",
	       (unsigned long long)st.lost, (unsigned long long)st.dup, (unsigned long long)st.backward,
	       (unsigned long long)st.crc_err, (unsigned long long)st.sync_err,
	       (unsigned long long)st.io_err, (unsigned long long)st.soft_resync);
	if (have_latest) {
		printf("BENCH ultima ventana: seq=%u preset=%u fs=%u Sa/s\n", latest.wseq,
		       latest.preset, latest.fs_hz);
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
	int port = 8081, bench_n = 0, rt = 0, set_preset = -1, opt;
	static const struct option lo[] = {
		{ "dev", 1, 0, 'd' }, { "speed", 1, 0, 's' }, { "gap-us", 1, 0, 'g' },
		{ "out", 1, 0, 'o' }, { "port", 1, 0, 'p' }, { "refresh", 1, 0, 'r' },
		{ "bench", 1, 0, 'b' }, { "bench-secs", 1, 0, 'T' }, { "preset", 1, 0, 'P' },
		{ "rt", 0, 0, 'R' }, { "verbose", 0, 0, 'v' }, { "demo", 0, 0, 'D' },
		{ 0, 0, 0, 0 } };

	while ((opt = getopt_long(argc, argv, "d:s:g:o:p:r:b:T:P:RvD", lo, NULL)) != -1) {
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
		case 'R': rt = 1; break;
		case 'v': verbose = 1; break;
		case 'D': demo = 1; break;
		default:
			fprintf(stderr, "uso: %s [--dev D] [--speed Hz] [--gap-us N] [--out F] [--port P]\n"
				"          [--refresh 0.25..1] [--preset 0..3] [--bench N] [--rt] [-v]\n", argv[0]);
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
	if (set_preset >= 0) {
		preset_req = set_preset;
		queue_command(OSC_CMD_SET_PRESET, (uint8_t)set_preset);
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
				fprintf(stderr, "seq=%u preset=%u fs=%u win=%llu lost=%llu crc=%llu\n",
					latest.wseq, latest.preset, latest.fs_hz,
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
