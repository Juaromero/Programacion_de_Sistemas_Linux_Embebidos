#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include "spi_master.h"

uint64_t now_ns(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

void sleep_until_ns(uint64_t t_ns)
{
	struct timespec ts = {
		.tv_sec = (time_t)(t_ns / 1000000000ull),
		.tv_nsec = (long)(t_ns % 1000000000ull),
	};

	while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL) == EINTR) {
	}
}

int spim_open(struct spi_master *m, const char *dev, uint32_t speed_hz, uint32_t gap_us)
{
	uint8_t mode = SPI_MODE_0, bits = 8;

	memset(m, 0, sizeof(*m));
	m->speed_hz = speed_hz;
	m->gap_us = gap_us;
	m->fd = open(dev, O_RDWR);
	if (m->fd < 0) {
		perror(dev);
		return -1;
	}
	if (ioctl(m->fd, SPI_IOC_WR_MODE, &mode) < 0 ||
	    ioctl(m->fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
	    ioctl(m->fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz) < 0) {
		perror("spidev ioctl");
		close(m->fd);
		return -1;
	}
	/* Releer lo que realmente quedo configurado (UT-SBC-01) */
	uint8_t rmode = 0xFF, rbits = 0;
	uint32_t rspeed = 0;

	ioctl(m->fd, SPI_IOC_RD_MODE, &rmode);
	ioctl(m->fd, SPI_IOC_RD_BITS_PER_WORD, &rbits);
	ioctl(m->fd, SPI_IOC_RD_MAX_SPEED_HZ, &rspeed);
	fprintf(stderr, "spidev %s: modo=%u bits=%u velocidad_max=%u Hz\n", dev, rmode, rbits,
		rspeed);
	return 0;
}

void spim_close(struct spi_master *m)
{
	if (m->fd >= 0) {
		close(m->fd);
		m->fd = -1;
	}
}

int spim_xfer(struct spi_master *m, const uint8_t tx[OSC_SEG_LEN], uint8_t rx[OSC_SEG_LEN],
	      uint64_t *ioctl_ns)
{
	/* Respetar el tiempo de rearme del esclavo entre transacciones */
	if (m->last_end_ns) {
		sleep_until_ns(m->last_end_ns + (uint64_t)m->gap_us * 1000ull);
	}
	struct spi_ioc_transfer tr;

	memset(&tr, 0, sizeof(tr));
	tr.tx_buf = (unsigned long)tx;
	tr.rx_buf = (unsigned long)rx;
	tr.len = OSC_SEG_LEN;
	tr.speed_hz = m->speed_hz;
	tr.bits_per_word = 8;

	uint64_t t0 = now_ns();
	int r = ioctl(m->fd, SPI_IOC_MESSAGE(1), &tr);
	uint64_t t1 = now_ns();

	m->last_end_ns = t1;
	if (ioctl_ns) {
		*ioctl_ns = t1 - t0;
	}
	return r < 0 ? -1 : 0;
}
