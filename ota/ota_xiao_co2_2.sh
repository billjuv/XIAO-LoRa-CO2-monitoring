#!/bin/bash
# OTA upload for LoRa_XIAO_CO2_2_B893A8D4DB1C (Nevada - K1W1 network)
# Update --ip address after deployment (reachable via Tailscale subnet router on Nevada Pi)
~/.platformio/penv/bin/python \
  ~/.platformio/packages/framework-arduinoespressif32@3.20014.231204/tools/espota.py \
  --ip 0.0.0.0 \
  --port 3232 \
  --auth YOUR_OTA_PASSWORD \
  --file .pio/build/seeed_xiao_esp32s3/firmware.bin