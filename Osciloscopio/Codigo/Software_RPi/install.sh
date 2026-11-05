#!/bin/bash
# Instalacion en la Raspberry Pi 4 (Raspberry Pi OS). Ejecutar con: sudo ./install.sh
set -euo pipefail
cd "$(dirname "$0")"

echo "[1/6] Paquetes"
apt-get update
apt-get install -y build-essential nginx

echo "[2/6] Habilitar SPI0 (/dev/spidev0.0) y fijar core_freq"
CFG=/boot/firmware/config.txt
[ -f "$CFG" ] || CFG=/boot/config.txt
grep -q '^dtparam=spi=on' "$CFG" || echo 'dtparam=spi=on' >> "$CFG"
# El reloj SPI deriva de core_freq: fijarlo evita que cambie con la carga de la CPU
grep -q '^core_freq=' "$CFG" || printf 'core_freq=500\ncore_freq_min=500\n' >> "$CFG"

echo "[3/6] Compilar y ejecutar pruebas unitarias del protocolo"
make clean && make && make test
install -m 0755 oscd /usr/local/bin/oscd

echo "[4/6] Sitio web"
install -d /var/www/osciloscopio
install -m 0644 web/index.html web/uPlot.iife.min.js web/uPlot.min.css web/uPlot.LICENSE.txt /var/www/osciloscopio/
install -m 0644 config/nginx-osciloscopio.conf /etc/nginx/sites-available/osciloscopio
ln -sf /etc/nginx/sites-available/osciloscopio /etc/nginx/sites-enabled/osciloscopio
rm -f /etc/nginx/sites-enabled/default
nginx -t && systemctl enable --now nginx && systemctl reload nginx

echo "[5/6] Servicio systemd"
install -m 0644 config/oscd.service /etc/systemd/system/oscd.service
systemctl daemon-reload
systemctl enable oscd.service

echo "[6/6] Listo. Reinicie (sudo reboot) si SPI o core_freq se acaban de habilitar;"
echo "      luego: sudo systemctl start oscd  y abra http://<ip-de-la-raspberry>/"
