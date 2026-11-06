# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(osciloscopio_esp32s3)

target_sources(app PRIVATE
    src/main.c
    src/protocol.c
    src/acq.c
    src/spi_link.c
)
target_include_directories(app PRIVATE src)
# Cabecera interna del driver ADC de Espressif: se usa para leer el bufer DMA crudo
# (canal y validez de cada conversion), que la API de Zephyr no expone.
target_include_directories(app PRIVATE ${ZEPHYR_BASE}/drivers/adc)
