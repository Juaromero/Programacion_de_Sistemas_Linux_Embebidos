CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -std=gnu11
SRC      = src/oscd.c src/protocol.c src/spi_master.c

all: oscd

oscd: $(SRC) src/protocol.h src/spi_master.h
	$(CC) $(CFLAGS) -o $@ $(SRC) -lm

# Pruebas unitarias que no requieren hardware (CRC, tramas, decodificacion)
test: tests/test_protocol
	./tests/test_protocol

tests/test_protocol: tests/test_protocol.c src/protocol.c src/protocol.h
	$(CC) $(CFLAGS) -Isrc -o $@ tests/test_protocol.c src/protocol.c

clean:
	rm -f oscd tests/test_protocol

.PHONY: all test clean
