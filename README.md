# XIAO/LoRa/SCD41 CO₂ Monitoring System

A wireless CO₂, temperature, and humidity monitoring system for mushroom grow operations, using LoRa radio to send sensor data to a central MQTT broker — no WiFi is required on the sensor node. In this project, the remote location is a mushroom grow facility in Nevada, and the local network names and device names used throughout are examples — substitute your own network and device names as appropriate.


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
- [SCD41 Calibration (FRC)](#scd41-calibration---frc)
- [OTA Firmware Updates](#ota-firmware-updates)
- [Known Issues & Lessons Learned](#known-issues--lessons-learned)
- [Future Enhancements](#future-enhancements)
  
---

## Overview

This project monitors CO₂ levels in the incubation and fruiting areas of a mushroom grow operation. At this time, three identical sensor units are deployed (though only two are documented below), each transmitting wirelessly over LoRa to a central gateway. The sensor nodes can be run on battery power and need to be moved outdoors for recalibration, or while determining optimal placement. 


**Why LoRa instead of WiFi?**  

WiFi has its challanges in the twin metal shipping containers in this mushroom grow space. I want to test the reliability and range of LoRa (not LoRaWAN) in this situation and hopefully be ready for future growth. Ultimately, I wish to expand this method to more remote areas of the farm as well. **Note:** The LilyGo T3 LoRa32 running OpenMQTTGateway does require WiFi to send the data recieved to the MQTT broker.

**Why Sensirion SCD41 instead of the Atlas Scientific EZO-CO₂?**  

The EZO sensors require lab-grade reference gases for recalibration and proved unreliable in this environment (one of two failed and the other EZO-CO2 sensor readings seem considerably lower than expected). The SCD41 supports Forced Recalibration (FRC) using outdoor air (~425 ppm), making field recalibration practical with just a short trip outside. Cost is also an obvious factor.
**Note:** The SCD41 has a maximum CO₂ reading of 5000 ppm, whereas the Atlas EZO-CO₂ max is 10,000 ppm. This should not be a problem in most cases as you want to keep the CO₂ level in fruiting well below 1000. During incubation, levels of 10,000 to 20,000 ppm are common - which is beyond the EZO's capabilities as well.


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

**Why InfluxDB/Grafana instead of Mycodo?** 

[Mycodo](https://kizniche.github.io/Mycodo/) could be installed in place of the InfluxDB/Grafana programs if desired. I started with Mycodo but found, in this operation, Node-RED and Flowfuse Dashboard 2.0 offered all the functionality needed with a better control dashboard on a mobile phone. All I ended up using Mycodo for was the built-in InfluxDB database and graphs. I switched over to stand-alone InfluxDB/Grafana programs for less overhead and prettier graphs.

---

## Bill of Materials

| Component | Part | Source |
|---|---|---|
| MCU + LoRa | Seeed Studio XIAO ESP32S3 + Wio-SX1262 LoRa B2B Kit | [Seeed Studio](https://www.seeedstudio.com/Wio-SX1262-with-XIAO-ESP32S3-p-5982.html?srsltid=AfmBOooeP_Vowj0WumESjsYdbNIdZAmZV2yhTu-AQrC7AMUwOGgeB1oY) |
| CO₂/Temp/RH Sensor | Adafruit SCD41 | [Adafruit](https://www.adafruit.com/product/5190?srsltid=AfmBOooh3oM7GaWI46jUPZLEENWeh_0F4iTKuAPATtS-eD_7WIwIb3Oz) |
| Battery Monitor | Adafruit MAX17048 LiPoly / LiIon Fuel Gauge | [Adafruit](https://www.amazon.com/dp/B0D7CL94L2?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_2&th=1) |
| Battery | 3.7V / 1800mAh LiPo | [Amazon](https://www.amazon.com/YELUFT-Rechargeable-Protection-Compatiable-Development/dp/B0F1J7359H/ref=sr_1_6?dib=eyJ2IjoiMSJ9.pjCXThguHmAiR_IywERGdQX_sX2OoZ8LrDLRn55ieJsPLxnohjzWqfsXzmy6yI89Cio3uj3URkKqd906E86h-bsiB7p1QrsNBCcXoGJMAiIq_fkGhcskxiDEmtuyEtM22uWWuC2B0I0D4K2GZigMdeSSa02vBxoLYtJ0UaVo2vC5byY8Br3Vdc4TetSTwc96JWHi6YdS-BLNf5VtgUK58i-5OWxropRLvZT5gBkSFIlT8_mCXCgrIBtZ_ksW86FfaSR0YwaJ1v6itVIjbylrMQusUokvB1SlLFNPtF4q6A0.EEyyXG95mttCLKSZHatFmJeCb-53s5JzMAHgB9dRdTo&dib_tag=se&keywords=lipo%2B1800mAh&qid=1776748293&sr=8-6&th=1) |
| LoRa Gateway | LilyGo T3 LoRa32 v1.6.1 (915mHz)| [AliExpress](https://www.aliexpress.us/item/2251832685763835.html?src=google&src=google&albch=shopping&acnt=231-612-1468&isdl=y&slnk=&plac=&mtctp=&albbt=Google_7_shopping&aff_platform=google&aff_short_key=_oFgTQeV&gclsrc=aw.ds&albagn=888888&ds_e_adid=&ds_e_matchtype=&ds_e_device=c&ds_e_network=x&ds_e_product_group_id=&ds_e_product_id=en2251832685763835&ds_e_product_merchant_id=109240285&ds_e_product_country=US&ds_e_product_language=en&ds_e_product_channel=online&ds_e_product_store_id=&ds_url_v=2&albcp=23333650963&albag=&isSmbAutoCall=false&needSmbHouyi=false&gad_source=1&gad_campaignid=23324517798&gbraid=0AAAABBR8kP2xOwxrKV5uOURVW8DMaRcxS&gclid=CjwKCAjwnZfPBhAGEiwAzg-VzLDYeBgHiBFgLTZA3s40C0xA6wWpEgt4EjpScHEgcRwQtzJ8_KSNaRoCItgQAvD_BwE&gatewayAdapt=glo2usa) |
| USB-C connector | Female waterproof Type-C panel mount | [Amazon](https://www.amazon.com/dp/B0D7CL94L2?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_2&th=1) |
| Waterproof On/Off Switch | 12mm push-on, push-off IP65 | [Amazon](https://www.amazon.com/dp/B0D5CVJGYF?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_4&th=1) |
| PTFE filter membranes | 20mm, 0.3µm pore, hydrophobic, adhesive-backed | [Amazon](https://www.amazon.com/dp/B09LD119PF?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_15&th=1) |
| Sensor cable | 22/4 Stranded/Shielded w/2.54 Female Dupont commectors| Shop parts |
| Enclosure (MCU/battery) | Custom 3D printed box | — |
| Enclosure (sensor) | Custom 3D printed box | — |


RE: Vendors - Seeed Studio and Adafruit are good sources for products linked, but [Digikey](https://www.digikey.com) carries these items as well at similar pricing, but web pages chosen were better.

> ⚠️ As with any Lora or RF device, be sure to attach the antenna prior to sending any signals. Damage can occur

> ⚠️ Check battery plug polarity (2.0 JST) - all I had received are reversed from normal. Easy to correct, but do it before you let the smoke out.

## Datasheets
[Sensirion SCD4x](Extras/Datasheets/Sensirion_SCD4x_Datasheet.pdf)

[Adafruit-SCD40/41](Extras/Datasheets/adafruit-scd-40-and-scd-41.pdf)

[Adafruit-MAX17048](Extras/Datasheets/adafruit-max17048-lipoly-liion-fuel-gauge-and-battery-monitor.pdf)

---

## Hardware Setup

### Enclosure Design

The sensor unit is split into two separate 3D-printed boxes connected by a short cable (under 1 meter):

Separating the sensor from the electronics keeps heat from the MCU and LoRa module away from the SCD41, which improves temperature reading accuracy.

<img src=Attachments/Photos/Both_Comp.jpeg width="50%"/>


- **Main box** — contains the XIAO ESP32S3 + Wio-SX1262, MAX17048, and LiPo battery(under PCB). Houses the waterproof USB-C connector for charging and wired power as well as a button to disconnect battery power if total shutdown is desired.

<img src=Attachments/Photos/XiAO_Top.jpeg width="50%"/>

(Threaded inserts and lid not shown)


- **Sensor box** — contains the SCD41 only, with a replaceable hydrophobic PTFE membrane filter over the sensor openings (not shown). This box can be positioned for optimal airflow independent of power constraints.

The design of the SCD enclosure takes into account the air flow recommendations from the Sensirion Datasheet.

<img src=Attachments/Photos/SCD_AirFlow.png width="50%"/>

The foam around the sensor improves the seal (below).

<img src=Attachments/Photos/SCD_Enc1.jpeg width="50%"/>

The cable enters from the top to allow hanging with the sensor facing down. The cable grommet shown is 3D printed out of TPU to keep out moisture. Stick-on PTFE membranes are to be placed over vent holes on sides and bottom and replaced as needed.

<img src=Attachments/Photos/SCD_Enc2.jpeg width="50%"/>

(Threaded inserts and end-cap not shown)


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

<img src=Attachments/Photos/Xiao_Board_Schematic.jpeg width="90%"/>
<img src=Attachments/Photos/XIAO_Batt_Board.jpg width="70%"/>

> **Note:** Gerber files located in Extras Folder (Extras/Gerber Files/XIAO_Gerber_PCB1.zip)


For other uses, board has additional access to XIAO GPIO pins if needed. Resistors for i2c (under XIAO) usually not needed but available. XIAO MCU board provides built-in battery charging circuitry. **Note:** The above board uses JST 2.0 connectors

- Battery life with the 1800mAh LiPo exceeded 15 hours in testing before recharging. I chose this battery to give enough life during initial optimal placement determination. In practice, the units are typically kept on wired USB-C power when stationary, with battery used mainly for outdoor FRC recalibration runs.

> ⚠️ LiPo batteries often have battery polarity reversed and can cause damage (easy to swap pins for the JST 2.0 connector).

<img src=Attachments/Photos/Sensor_Encl.jpg width="70%"/>

### Enclosures & 3D Printing
Both enclosures, the cable grommet, and the lid gasket are custom 3D-printed parts. Full documentation including Onshape links, print settings, materials, and assembly notes is in [ENCLOSURES.md](Enclosures.md).

---

## Firmware

### Prerequisites

- [VSCode](https://code.visualstudio.com/) with [PlatformIO extension](https://platformio.org/)
- A USB-C cable for initial flashing

### Repository Files
- src/main.cpp — Main firmware source
- platformio.ini — PlatformIO build configuration


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

> ⚠️ Using only the last 3 MAC bytes caused duplicate names on boards from the same batch. The full 6-byte MAC is required. Note that the MAC bytes are reported in reverse order — the actual MAC address for the example above would be 1C:DB:D4:A7:02:2C.


### Key Firmware Behaviors

| Behavior | Details |
|---|---|
| Measurement interval | 31 seconds (odd numbers to avoid possible packet collisions between units)*|
| ASC (auto-calibration) | Disabled on every boot and persisted to EEPROM |
| LoRa TX mode | Asynchronous — sensor can still receive commands while transmitting |
| Battery monitoring | MAX17048 detected at startup; if absent, battery fields report 0 / UNKNOWN |
| CPU frequency | Locked to 240 MHz — prevents light sleep from interfering with LoRa interrupts |
| FRC duplicate guard | Ignores repeated FRC commands with the same target within 30 seconds |

*During development, 9 seconds (being a non-round number) was found to make accidental sync less likely to persist even if boards start up at the same time. Time was increased to 31 seconds for production.

---

### Remote Commands (via LoRa)

Commands are sent from Node-RED through the OMG gateway as JSON, targeted by device name:

| Command | JSON payload example |
|---|---|
| Forced recalibration | `{"target":"LoRa_XIAO1_2C02A7D4DB1C","frc":420}` |
| Set temperature offset | `{"target":"LoRa_XIAO1_2C02A7D4DB1C","temp_offset":2.5}` |
| Get temperature offset | `{"target":"LoRa_XIAO1_2C02A7D4DB1C","get_temp_offset":true}` |
| Set altitude | `{"target":"LoRa_XIAO1_2C02A7D4DB1C","altitude":1500}` |
| Get altitude | `{"target":"LoRa_XIAO1_2C02A7D4DB1C","get_altitude":true}` |
| Trigger OTA update | `{"target":"LoRa_XIAO1_2C02A7D4DB1C","ota":true}` |

> The OMG gateway wraps outgoing commands in a `message` field. The firmware handles both wrapped and unwrapped formats for flexibility.

---

### Flashing

1. Connect the XIAO via USB-C
2. Open the project in VSCode/PlatformIO
3. Click **Upload** (or `pio run -t upload`)
4. Open Serial Monitor at 115200 baud to verify startup

---

## LoRa Gateway (OMG)

**Hardware:** LilyGo T3 LoRa32 v1.6.1  
**Firmware:** [OpenMQTTGateway](https://docs.openmqttgateway.com/) v1.8.1

OMG Configuration
The LilyGo T3 LoRa32 was flashed with OpenMQTTGateway v1.8.1 using their web installer — no Arduino IDE or PlatformIO required. Key configuration settings:

| Setting | Value |
|---|---|
| MQTT Broker | MushRmPi (Raspberry Pi running Mosquitto)
| Topic Prefix | OMGhome 
| Gateway Name | OMG_LORA_MOAPA
| WiFi | Required for MQTT connection to broker


The LoRa radio parameters (frequency, SF, bandwidth, etc.) are configured via MQTT commands after flashing — see the OMG LoRa documentation for details. The parameters used in this project are listed in the LoRa Radio Parameters table below.


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

> **Sync word 0x12** keeps traffic off from public LoRaWAN networks. Do not use 0x34.

---

## MQTT Data Pipeline

### Incoming Telemetry (OMG → MQTT)

OMG publishes recieved packets to:
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
  "crc_errors": 42
}
```
**Note:** CRC errors in LoRa telemetry
Each sensor unit tracks and reports cumulative CRC errors — packets received but discarded due to radio corruption or collision. Some level of CRC errors is normal, particularly in metal-walled environments. High counts relative to valid packets received may indicate range, interference, or collision issues worth investigating.


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

<img src=Attachments/Photos/NR_Sensor.jpg width="70%"/>

<img src=Attachments/Photos/NR_FRC_Set.jpg width="70%"/>

<img src=Attachments/Photos/SCD41_TempAltitude_Calibration_and_OTA_Trigger.jpeg width="70%"/>

**JSON files for all three elements are located in "Extras/NodeRED" folder** - 

Import these via Node-RED menu → Import → select file

**Platform:** Node-RED v3.1.7 with FlowFuse Dashboard 2.0

### Features

- **Per-sensor dashboard panels** — separate groups for each CO₂ unit (XIAO1, XIAO2)
- **Live CO₂, temp, and RH readings**
- **Battery status display** — voltage, percent, charge rate
- **FRC calibration controls** — countdown timer, configurable target ppm
- **Multi-send reliability** — FRC command sent 3–4 times at 2-second intervals to account for LoRa packet loss
- **Watchdog status indicator** — LED goes yellow at 5 min silence, red at 10 min, with "Last seen X min ago" text
- **Temperature and Altitude calibration** - Converted for "F" and feet. Separate group as likely only done once
- **OTA trigger button** — select target device from dropdown and press "Trigger OTA Update Period" to initiate remote firmware update

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

**Sample Grafana Visualization:** (pre-deployment)

<img src=Attachments/Photos/Grafana_sample.jpg width="100%"/>


- Additional visualizations not set up yet, but could include Time-series panels for CO₂, temperature, and humidity per sensor

---

## SCD41 Calibration - FRC
[(See the Calibration.md file for more info)](CalibrationGuide.md)

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
4. **Note the outdoor CO₂ reading** — typical outdoor air is 420–435 ppm. If possible, check for nearby air monitoring web portals for a current reference value 
5. **Enter the target value** — on the Node-RED FRC panel, enter the current outdoor CO₂ reference value (typically 420 ppm)
6. **Start the countdown** — the dashboard timer will count down and then send the FRC command automatically (sent 3–4 times for reliability)
7. **Confirm success** — the sensor will transmit an `frc_applied: true` confirmation. The dashboard will display the result
8. **Return the sensor** to the grow room and allow 20–30 minutes for readings to stabilize

> **Important:** The sensor must be in stable outdoor conditions *before* the FRC command is sent (5 minutes minimum). The dashboard waits 15 seconds before executing the calibration after receiving the command. The new readings will not be immediately noticable, it will take several minutes to stablilize.

### Reference CO₂ Values

| Condition | Expected CO₂ |
|---|---|
| Outdoor air (typical) | 420–435 ppm |
| Indoor office/home | 600–1000 ppm |
| Mushroom incubation room | 10000–20000+ ppm* |
| Mushroom fruiting room | 500–1500 ppm (species-dependent) |
*Values commonly mentioned on Internet as optimum - unverified

---

## OTA Firmware Updates
[(See the OTA_Guide.md file for full instructions)](OTA_Guide.md)

Once deployed, firmware updates can be pushed to any sensor unit remotely — no USB cable or physical access required. A LoRa command sent from Node-RED triggers a 3-minute WiFi OTA window on the target device. The update is then pushed from a terminal or command prompt using a shell script in the `ota/` folder. The device reboots automatically and resumes normal LoRa operation.

The Node-RED Sensor Calibration dashboard includes a **Trigger OTA Update Period** button that works alongside the existing device selector — select the target unit and press the button. A status confirmation appears on the dashboard when the device has opened its OTA window.

For remote Nevada devices, updates are delivered over Tailscale via the Nevada Raspberry Pi subnet router — no site visit needed.

> See `ota/` folder for per-device upload scripts and `include/secrets.h.example` for WiFi and OTA password configuration.

---

## Known Issues & Lessons Learned

**Duplicate device names from same-batch boards**  
The full 6-byte MAC is used for device naming, but the bytes are reported in reverse order compared to what your router or network tools will show. Using only the last 3 bytes of the MAC address caused two boards to get identical names. Fixed by using the full 6-byte MAC.

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

**OTA via USB boot window not reliable**  
Initial OTA attempts using a timed WiFi window at boot were abandoned due to conflicts between the PlatformIO serial monitor and the USB port during upload. Replaced with LoRa-triggered OTA, which is more reliable and better suited to remote deployment.

---

## Future Enhancements

- Additional sensor units for expanded grow area coverage
- Grafana dashboards for long-term CO₂ trend analysis

---

*Documentation by BillJuv — part of the Nevada mushroom grow monitoring project.*  
*Public project documentation: [billjuv.github.io](https://billjuv.github.io)*