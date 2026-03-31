# SCD41 Calibration Guide

This guide covers the recalibration procedure for the XIAO/LoRa/SCD41 CO₂ sensor units used in the mushroom grow monitoring system.

---

## Background: Why FRC Instead of ASC?

The SCD41 sensor supports two calibration modes:

**Automatic Self-Calibration (ASC)** — The sensor automatically adjusts its baseline over time by assuming that the lowest CO₂ reading seen over a rolling 7-day window represents fresh outdoor air (~420 ppm). This works well in office or home environments where the sensor regularly sees outdoor-level air.

**Forced Recalibration (FRC)** — A manual one-time calibration performed by placing the sensor in air of a known CO₂ concentration (typically outdoors) and sending a reference value command.

**ASC is not suitable for this installation.** The sensors are deployed inside sealed mushroom grow containers where CO₂ levels regularly exceed 1000–5000 ppm and the sensor rarely if ever sees outdoor air during normal operation. ASC would cause the sensor to gradually drift its baseline upward, resulting in falsely low readings over time.

ASC is disabled in firmware on every boot and the setting is saved to the sensor's internal EEPROM so it survives power cycles.

---

## How Often to Recalibrate

Recalibrate every **7–10 days** under normal operation.

Also recalibrate if you notice:
- CO₂ readings that seem unusually high or low at rest
- A significant and growing difference between the two sensor units
- Readings that don't respond as expected when the grow room is ventilated
- After any extended power outage or sensor transport

---

## What You Need

- The sensor unit (XIAO + SCD41 assembly) with sufficient battery charge
- A phone or tablet with access to the Node-RED dashboard
- Access to outdoor air — an open area away from vehicles, HVAC exhausts, generators, or any combustion source
- A calm day is ideal; light wind is fine

---

## Step-by-Step FRC Procedure

### 1 — Check battery before going outdoors

On the Node-RED dashboard, check the battery percentage for the sensor unit you are calibrating.

- **Green / OK:** Proceed
- **Yellow / WARNING (below 20%):** Charge before calibrating if possible; the process takes 10–15 minutes outdoors
- **Red / CRITICAL (below 10%):** Charge first — do not risk the sensor losing power mid-calibration

### 2 — Take the sensor unit outdoors

Carry the sensor to an open outdoor location. Good spots:
- Away from building walls (HVAC exhaust near rooflines)
- Away from any running vehicles or generators
- At least a few feet off the ground (CO₂ is heavier than air and can pool near the ground)

### 3 — Wait for the reading to stabilize

Watch the live CO₂ reading on the dashboard. After being moved outdoors it will take several minutes to settle.

- **Wait at least 5–10 minutes** before proceeding
- You are looking for a stable reading in the **415–425 ppm** range
- If readings are higher (500+ ppm), move to a more open location and wait longer
- If readings are jumping around, allow more time to stabilize

> Typical outdoor CO₂ is currently around 420–425 ppm globally. Readings slightly above or below this are normal depending on location and conditions.

### 4 — Enter the FRC target value

On the Node-RED dashboard, navigate to the FRC panel for the sensor you are calibrating.

- Enter the **target CO₂ value** — use the current outdoor reading shown on the dashboard, rounded to the nearest 5 or 10 ppm (e.g., if reading 418 ppm, enter 420)
- Double-check you are sending to the correct sensor unit (XIAO1 or XIAO2)

### 5 — Start the calibration countdown

Press the **Start FRC** button on the dashboard.

- A countdown timer will run
- At zero, the FRC command is automatically sent **3–4 times** in rapid succession to account for possible LoRa packet loss
- The LED on the XIAO board will blink briefly when each command is received

### 6 — Confirm success

Within a few seconds of the command being sent, the sensor will transmit a confirmation packet. The dashboard will display:

| Result | Meaning |
|---|---|
| `frc_applied: true` | Calibration succeeded and was saved to EEPROM |
| `frc_applied: false` | Calibration command was received but failed — retry |
| No response | Command was not received — check LoRa signal strength (RSSI) and retry |

> If you are far from the grow facility and RSSI is weak (below −100 dBm), move closer to improve reception before retrying.

### 7 — Return the sensor and allow stabilization

- Return the sensor unit to its mounting location in the grow room
- Allow **20–30 minutes** for readings to stabilize before relying on the data
- The first few readings after returning to a high-CO₂ environment may be elevated as the sensor re-equilibrates

---

## Reference CO₂ Levels

| Environment | Typical CO₂ Range |
|---|---|
| Outdoor air | 415–425 ppm |
| Indoor home / office | 600–1000 ppm |
| Occupied room, poor ventilation | 1000–2000 ppm |
| Mushroom fruiting room | 500–1500 ppm (species-dependent) |
| Mushroom incubation / colonization | 2000–10000+ ppm |

---

## Temperature Offset Adjustment

The SCD41 measures temperature using an internal sensor. Because the MCU and LoRa radio generate heat inside the enclosure, the temperature reading may read slightly higher than actual ambient air temperature.

If you find that temperature readings are consistently offset from a known-good reference thermometer, a temperature offset can be set remotely from Node-RED without taking the sensor outdoors.

This is set in the FRC dashboard under **Temperature Offset**. A positive offset value compensates for heat from the enclosure (e.g., an offset of 2.5°C will subtract 2.5° from all reported temperature values). The setting is persisted to EEPROM.

---

## Altitude Compensation

The SCD41 uses barometric pressure compensation internally to improve CO₂ accuracy. If altitude is not set, it defaults to sea level (0 m), which will cause slightly elevated CO₂ readings at higher elevations.

The Nevada facility is at approximately **[fill in elevation in meters]** meters above sea level. This value is set once via the Node-RED dashboard under **Altitude Setting** and persisted to EEPROM. It does not need to be changed unless the sensor is relocated.

---

## Troubleshooting

**Dashboard shows no response after sending FRC**
- Verify the sensor is online (watchdog LED is green)
- Check RSSI on the dashboard — if below −100 dBm, move closer to the gateway
- The command is sent 3–4 times automatically; wait 10 seconds before retrying

**CO₂ reading doesn't change after FRC**
- The sensor needs 20–30 minutes to stabilize after returning indoors — this is normal
- If still unchanged after 30 minutes, check `frc_applied` status in the dashboard

**Outdoor reading won't stabilize below 500 ppm**
- Move further from buildings, parking areas, or any combustion sources
- On still days, CO₂ can pool in low-lying or sheltered areas — try a slightly elevated or more exposed spot

**Both sensors drift together in the same direction**
- This is a good sign — it suggests the environment is changing, not calibration error
- If both are reading low, verify the grow room CO₂ source is working as expected
- If both are reading high, check for CO₂ buildup from substrate or increasing biological activity

---

*Part of the Nevada mushroom grow CO₂ monitoring project.*
*Full project documentation: [billjuv.github.io](https://billjuv.github.io)*
