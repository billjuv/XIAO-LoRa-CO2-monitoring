#!/bin/bash
# OTA upload for LoRa_XIAO1 (local - Ripple network)
~/.platformio/penv/bin/python \
  ~/.platformio/packages/framework-arduinoespressif32@3.20014.231204/tools/espota.py \
  --ip 10.0.1.113 \
  --port 3232 \
  --auth LRTait4608 \
  --timeout 30 \
  --file .pio/build/seeed_xiao_esp32s3/firmware.bin