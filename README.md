# XIAO/LoRa/SCD41 CO₂ Monitoring System

A wireless CO₂, temperature, and humidity monitoring system for mushroom grow operations, using LoRa radio to send sensor data to a central MQTT broker — no WiFi required on the sensor node.

> 📸 *Photos coming soon*

---

## Table of Contents

- [Overview](#overview)
- [Bill of Materials](#bill-of-materials)
- [Hardware Setup](#hardware-setup)
- [Firmware](#firmware)
- [LoRa Gateway (OMG)](#lora-gateway-omg)
- [MQTT Data Pipeline](#mqtt-data-pipeline)
- [Node-RED Dashboard](#node-red-dashboard)
- [InfluxDB & Grafana](#influxdb--grafana)
- [SCD41 Calibration (FRC)](#scd41-calibration-frc)
- [Known Issues & Lessons Learned](#known-issues--lessons-learned)
- [Future Enhancements](#future-enhancements)
  
---

## Overview

This project monitors CO₂ levels in the incubation and fruiting areas of a mushroom grow operation. Two identical sensor units are deployed, each transmitting wirelessly over LoRa to a central gateway.

**Why LoRa instead of WiFi?**  
The sensor nodes run on battery power and need to be moved for recalibration. LoRa allows untethered placement anywhere in the grow facility without needing WiFi credentials or network infrastructure changes.

**Why SCD41 instead of the Atlas Scientific EZO-CO₂?**  
The EZO sensors require lab-grade reference gases for recalibration and proved unreliable in this environment. The SCD41 supports Forced Recalibration (FRC) using outdoor air (~420 ppm), making field recalibration practical with just a short trip outside.

### System Architecture

```
[XIAO ESP32S3 + SCD41]
        |
        | LoRa (915 MHz)
        ↓
[LilyGo T3 LoRa32 running OpenMQTTGateway]
        |
        | MQTT (WiFi)
        ↓
[Mosquitto on Raspberry Pi 4]
        |
        ├── Node-RED  (dashboard, FRC commands, watchdog alerts)
        └── InfluxDB  (time-series storage → Grafana)
```

---

## Bill of Materials

| Component | Part | Source | Approx. Cost |
|---|---|---|---|
| MCU + LoRa | Seeed Studio XIAO ESP32S3 + Wio-SX1262 LoRa B2B Kit | Seeed Studio | *[fill in]* |
| CO₂/Temp/RH Sensor | Adafruit SCD41 | Adafruit | *[fill in]* |
| Battery Monitor | Adafruit MAX17048 LiPoly / LiIon Fuel Gauge | Adafruit | *[fill in]* |
| Battery | 3.7V / 1800mAh LiPo | *[fill in]* | *[fill in]* |
| LoRa Gateway | LilyGo T3 LoRa32 v1.6.1 | *[fill in]* | *[fill in]* |
| Enclosure (MCU/battery) | Custom 3D printed box | — | filament cost |
| Enclosure (sensor) | Custom 3D printed box | — | filament cost |
| Sensor cable | *[fill in — length, connector type]* | *[fill in]* | *[fill in]* |
| Waterproof USB-C connector | Female waterproof Type-C panel mount | *[fill in]* | *[fill in]* |
| PTFE filter membranes | 20mm, 0.3µm pore, hydrophobic, adhesive-backed | *[fill in]* | *[fill in]* |

**Tools needed:**
- Soldering iron and solder
- 3D printer (for enclosures)
- Computer with PlatformIO/VSCode for flashing firmware

---

## Hardware Setup

### Enclosure Design

The sensor unit is split into two separate 3D-printed boxes connected by a short cable (under 1 meter):

- **Sensor box** — contains the SCD41 only, with a replaceable hydrophobic PTFE membrane filter over the sensor opening. This box can be positioned for optimal airflow independent of power constraints.
- **Main box** — contains the XIAO ESP32S3 + Wio-SX1262, MAX17048, and LiPo battery. Houses the waterproof USB-C connector for charging and wired power.

Separating the sensor from the electronics keeps heat from the MCU and LoRa module away from the SCD41, which improves temperature reading accuracy.

> 📸 *Enclosure photos and 3D print files coming soon*

### Pin Mapping

| Function | XIAO ESP32S3 Pin |
|---|---|
| SX1262 NSS (chip select) | 41 |
| SX1262 DIO1 | 39 |
| SX1262 RST | 42 |
| SX1262 BUSY | 40 |
| Status LED | 48 |
| SCD41 SDA (I2C) | 5 (default) |
| SCD41 SCL (I2C) | 6 (default) |
| MAX17048 SDA | 5 (shared I2C bus) |
| MAX17048 SCL | 6 (shared I2C bus) |

> **Note:** The XIAO ESP32S3 + Wio-SX1262 B2B Kit uses a custom pin mapping for the SX1262 that differs from separately purchased boards. The values above are specific to the B2B kit variant.

### Wiring / Schematic

> 📐 *Schematic and Gerber files coming soon*

---

## Firmware

### Prerequisites

- [VSCode](https://code.visualstudio.com/) with [PlatformIO extension](https://platformio.org/)
- A USB-C cable for initial flashing

### platformio.ini

```ini
[env:seeed_xiao_esp32s3]
platform = espressif32@6.6.0
board = seeed_xiao_esp32s3
framework = arduino
monitor_speed = 115200
monitor_filters = time
build_flags =
    -DCORE_DEBUG_LEVEL=5
board_build.stack_size = 16384
board_build.f_cpu = 240000000L
board_build.f_flash = 80000000L
lib_deps =
    Sensirion I2C SCD4x @ ^1.0.0
    bblanchon/ArduinoJson @ ^7.4.2
    sensirion/Sensirion Core @ ^0.7.2
    jgromes/RadioLib @ ^7.5.0
    adafruit/Adafruit MAX1704X @ ^1.0.2
```

### Device Naming

Each unit auto-generates a unique name from its full 6-byte MAC address. Set the base name at the top of `main.cpp`:

```cpp
#define DEVICE_BASE_NAME "LoRa_XIAO1"   // Change for each unit
```

The result will be something like `LoRa_XIAO1_2C02A7D4DB1C`. This name is used as the MQTT `value` field and as the target address for remote commands.

> ⚠️ Using only the last 3 MAC bytes caused duplicate names on boards from the same batch. The full 6-byte MAC is required.

### Key Firmware Behaviors

| Behavior | Details |
|---|---|
| Measurement interval | 9 seconds (offset from 10s to avoid packet collisions between units) |
| ASC (auto-calibration) | Disabled on every boot and persisted to EEPROM |
| LoRa TX mode | Asynchronous — sensor can still receive commands while transmitting |
| Battery monitoring | MAX17048 detected at startup; if absent, battery fields report 0 / UNKNOWN |
| CPU frequency | Locked to 240 MHz — prevents light sleep from interfering with LoRa interrupts |
| FRC duplicate guard | Ignores repeated FRC commands with the same target within 30 seconds |

### Remote Commands (via LoRa)

Commands are sent from Node-RED through the OMG gateway as JSON, targeted by device name:

| Command | JSON payload example |
|---|---|
| Forced recalibration | `{"target":"LoRa_XIAO1_2C02A7D4DB1C","frc":420}` |
| Set temperature offset | `{"target":"LoRa_XIAO1_2C02A7D4DB1C","temp_offset":2.5}` |
| Get temperature offset | `{"target":"LoRa_XIAO1_2C02A7D4DB1C","get_temp_offset":true}` |
| Set altitude | `{"target":"LoRa_XIAO1_2C02A7D4DB1C","altitude":1500}` |
| Get altitude | `{"target":"LoRa_XIAO1_2C02A7D4DB1C","get_altitude":true}` |

> The OMG gateway wraps outgoing commands in a `message` field. The firmware handles both wrapped and unwrapped formats for flexibility.

### Flashing

1. Connect the XIAO via USB-C
2. Open the project in VSCode/PlatformIO
3. Click **Upload** (or `pio run -t upload`)
4. Open Serial Monitor at 115200 baud to verify startup

---

## LoRa Gateway (OMG)

**Hardware:** LilyGo T3 LoRa32 v1.6.1  
**Firmware:** [OpenMQTTGateway](https://docs.openmqttgateway.com/) v1.8.1

### LoRa Radio Parameters

Both the sensor firmware and OMG must use identical settings:

| Parameter | Value |
|---|---|
| Frequency | 915.0 MHz (US) |
| Spreading Factor | SF7 |
| Bandwidth | 125 kHz |
| Coding Rate | 4/5 |
| Preamble Length | 8 |
| Sync Word | 0x12 (private network) |
| CRC | Enabled |

> **Sync word 0x12** keeps traffic off public LoRaWAN networks. Do not use 0x34.

---

## MQTT Data Pipeline

### Incoming Telemetry (OMG → MQTT)

OMG publishes received packets to:
```
OMGhome/OMG_ESP32_LORA/LORAtoMQTT
```

Example payload:
```json
{
  "value": "LoRa_XIAO1_2C02A7D4DB1C",
  "co2": 587,
  "temp": 20.77,
  "hum": 35.36,
  "rssi": -38,
  "snr": 10.0,
  "batt_volt": 4.09,
  "batt_percent": 90.2,
  "batt_crate": -3.95,
  "batt_status": "OK",
  "crc_errors": 0
}
```

### Node-RED Parsed Topics

Node-RED subscribes to the OMG topic, parses by device name, and republishes to per-device topics:

```
LoRa_NodeRed/LoRa_XIAO1_2C02A7D4DB1C/co2
LoRa_NodeRed/LoRa_XIAO1_2C02A7D4DB1C/temp
LoRa_NodeRed/LoRa_XIAO1_2C02A7D4DB1C/hum
LoRa_NodeRed/LoRa_XIAO1_2C02A7D4DB1C/batt_percent
... etc.
```

### Outgoing Commands (Node-RED → OMG → LoRa)

Node-RED publishes FRC and calibration commands to:
```
OMGhome/commands/MQTTtoLORA
```

Wrapped payload format required by OMG:
```json
{"message":"{\"target\":\"LoRa_XIAO1_2C02A7D4DB1C\",\"frc\":420}"}
```

---

## Node-RED Dashboard

**Platform:** Node-RED v3.1.7 with FlowFuse Dashboard 2.0

### Features

- **Per-sensor dashboard panels** — separate groups for each CO₂ unit (XIAO1, XIAO2)
- **Live CO₂, temp, and RH readings**
- **Battery status display** — voltage, percent, charge rate
- **FRC calibration controls** — countdown timer, configurable target ppm
- **Multi-send reliability** — FRC command sent 3–4 times at 2-second intervals to account for LoRa packet loss
- **Watchdog status indicator** — LED goes yellow at 5 min silence, red at 10 min, with "Last seen X min ago" text

> 📸 *Dashboard screenshot coming soon*

> 📄 *Node-RED flow export (.json) coming soon*

---

## InfluxDB & Grafana

**InfluxDB version:** 1.x  
**Database:** `Sensors`  
**Host:** MushRmPi (Raspberry Pi 4)

### Data Write Method

Data is written to InfluxDB via an **HTTP request node** in Node-RED (not the InfluxDB Out node). This gives direct control over the line protocol format and avoids compatibility issues with InfluxDB 1.x.

### Measurements & Fields

| Measurement | Fields |
|---|---|
| `LoRa_XIAO1_co2` | `co2`, `hum`, `temp` |
| `LoRa_XIAO2_co2` | `co2`, `hum`, `temp` |

> ⚠️ Spaces in InfluxDB measurement names cause silent write failures in line protocol. Use underscores only.

### Grafana

- Time-series panels for CO₂, temperature, and humidity per sensor
- **State Timeline panel** used to passively monitor for simultaneous dropout of both XIAO units

---

## SCD41 Calibration (FRC)

The SCD41 uses **Forced Recalibration (FRC)** rather than Automatic Self-Calibration (ASC). ASC assumes regular exposure to fresh outdoor air (~420 ppm), which does not happen in a sealed mushroom grow environment. ASC is disabled in firmware and the setting is persisted to EEPROM.

### When to Recalibrate

Recalibrate every **7–10 days**, or any time readings seem inconsistent.

Signs calibration may have drifted:
- CO₂ readings are unusually high or low at rest
- Readings between the two units diverge significantly
- Indoor readings match or exceed expected fruiting-room levels even when the room has been aired out

### FRC Procedure

1. **Check battery** — ensure adequate charge before going outdoors (dashboard battery indicator)
2. **Take the sensor unit outdoors** — find an open area away from vehicle exhaust, HVAC vents, and any CO₂ sources; ideally a light wind
3. **Wait 5–10 minutes** — allow the sensor to equilibrate to outdoor air. Watch the live CO₂ reading on the dashboard stabilize
4. **Note the outdoor CO₂ reading** — typical outdoor air is 415–425 ppm. If readings are in this range, proceed. If far off, allow more stabilization time
5. **Enter the target value** — on the Node-RED FRC panel, enter the current outdoor CO₂ reference value (typically 420 ppm)
6. **Start the countdown** — the dashboard timer will count down and then send the FRC command automatically (sent 3–4 times for reliability)
7. **Confirm success** — the sensor will transmit an `frc_applied: true` confirmation. The dashboard will display the result
8. **Return the sensor** to the grow room and allow 20–30 minutes for readings to stabilize

> **Important:** The sensor must be in stable outdoor conditions *before* the FRC command is sent. The firmware waits 5 seconds before executing the calibration after receiving the command, but the sensor should already be equilibrated by that point.

### Reference CO₂ Values

| Condition | Expected CO₂ |
|---|---|
| Outdoor air (typical) | 415–425 ppm |
| Indoor office/home | 600–1000 ppm |
| Mushroom incubation room | 1000–5000+ ppm |
| Mushroom fruiting room | 500–1500 ppm (species-dependent) |

---

## Known Issues & Lessons Learned

**Duplicate device names from same-batch boards**  
Using only the last 3 bytes of the MAC address caused two boards to get identical names. Fixed by using the full 6-byte MAC.

**Spaces in InfluxDB measurement names**  
Measurement names containing spaces cause silent write failures in InfluxDB line protocol. Use underscores throughout.

**ASC re-enables after reboot if not persisted**  
`setAutomaticSelfCalibrationEnabled(0)` must be followed by `persistSettings()` or ASC will re-enable on next power cycle.

**FRC must be sent multiple times**  
LoRa packet loss probability is non-trivial. The Node-RED dashboard sends the FRC command 3–4 times at 2-second intervals. The firmware deduplicates these with a 30-second same-target guard.

**LoRa interrupts unreliable on battery without CPU lock**  
Automatic CPU frequency scaling and light sleep interfered with the RadioLib interrupt-driven RX on battery power. Fixed by locking CPU to 240 MHz at boot.

**Simultaneous dropout of both units**  
Both XIAO units occasionally dropped off at the same time. Monitored via a Grafana State Timeline panel. Resolved — was a fluke related to power cycling and battery use at the time, not an ongoing issue.

---

## Future Enhancements

- OTA firmware updates triggered via LoRa command, using WiFi over Tailscale for remote flashing without requiring a site visit to Nevada.
- Temperature offset and altitude compensation controls added to the Node-RED FRC dashboard.

---

*Documentation by Bill Juv — part of the Nevada mushroom grow monitoring project.*  
*Public project documentation: [billjuv.github.io](https://billjuv.github.io)*
