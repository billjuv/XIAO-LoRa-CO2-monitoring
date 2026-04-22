# OTA Firmware Update Guide
## XIAO LoRa CO2 Sensor Nodes

---

## Overview

These sensor nodes use **LoRa-triggered OTA** (Over-The-Air) firmware updates. This means:

- No USB cable or physical access to the device is needed once deployed
- A command sent from Node-RED via LoRa tells the target device to open a WiFi update window
- You push the new firmware from your computer using a Terminal command
- The device reboots automatically and resumes normal LoRa operation

### Multiple Devices

Each sensor node has a unique device name made up of a `DEVICE_BASE_NAME` (set before flashing, e.g. `LoRa_XIAO1`) plus the board's full MAC address (e.g. `LoRa_XIAO1_2C02A7D4DB1C`). This name is used to target OTA commands at a specific unit — other units on the same LoRa network will ignore commands not addressed to them.

Each device has its own OTA upload script in the `ota/` folder. You flash one device at a time.

### Multiple WiFi Networks

The firmware supports two WiFi networks. On boot into OTA mode, it tries the first network (`WIFI_SSID_1`) and falls back to the second (`WIFI_SSID_2`) if the first is not found. This allows the same firmware to work at different locations — for example, a home lab and a remote grow facility — without modification.

### Remote Updates via Tailscale

The XIAO sensor nodes themselves cannot run Tailscale. Instead, a Raspberry Pi at the remote location runs Tailscale with **subnet routing** enabled. This makes all devices on the remote local network reachable from your Mac over Tailscale, using their normal local IP addresses. You do not need to know any Tailscale-specific IP for the XIAO units — just their regular local network IP addresses, accessed through the subnet router.

---

## Platform Notes

### Mac / Linux
Terminal commands in this guide work as-is. The `chmod` command is required to make scripts executable.

### Windows
- Use **Command Prompt** or **PowerShell** instead of Terminal
- Replace `./ota/ota_xiao1.sh` with `python ota\ota_xiao1.py` (you will need to create `.py` versions of the scripts, or run the python command directly)
- The `chmod` command is not needed on Windows
- **Windows Subsystem for Linux (WSL)** users can follow the Mac/Linux instructions exactly

---

## One-Time Setup

### 1. Create your secrets.h file

Copy the example file and fill in your credentials:

```
include/secrets.h.example  →  include/secrets.h
```

Edit `include/secrets.h` with your actual values:

```cpp
#pragma once

#define WIFI_SSID_1     "YourHomeNetwork"
#define WIFI_SSID_2     "YourRemoteNetwork"
#define WIFI_PASSWORD_1 "your_home_password"
#define WIFI_PASSWORD_2 "your_remote_password"
#define OTA_PASSWORD    "your_ota_password"
```

> ⚠️ `secrets.h` is listed in `.gitignore` and will never be committed to the repo. Keep it private. Do not share it or post it anywhere.

`WIFI_SSID_1` should be the network your computer is on during development. `WIFI_SSID_2` is the fallback — typically the remote location network. The firmware tries SSID_1 first, then SSID_2.

---

### 2. Make the OTA scripts executable (Mac / Linux only)

This only needs to be done once after cloning the repo. Open Terminal and navigate to the project folder:

```bash
cd /path/to/XIAO-LoRa-CO2-monitoring
```

Then run:

```bash
chmod +x ota/ota_xiao1.sh
chmod +x ota/ota_xiao_co2_1.sh
chmod +x ota/ota_xiao_co2_2.sh
chmod +x ota/ota_xiao_co2_3.sh
```

If you see `permission denied` when running a script later, repeat the `chmod` command for that script.

---

### 3. Update the OTA scripts with correct IP addresses

Each script in the `ota/` folder contains the IP address for one device. Open each script and verify or update the `--ip` value:

- **Local devices:** Use the device's local IP address. Check your router's device list or use a network scanner app (e.g. LanScan on Mac) to find it. Setting a DHCP reservation in your router for each device's MAC address will keep the IP stable across reboots.
- **Remote devices:** Use the device's local IP address on the remote network. The remote Raspberry Pi runs Tailscale with subnet routing enabled, which makes these local IPs reachable from your Mac over Tailscale without any changes to the XIAO firmware or scripts.

---

### 4. Flash each unit with its unique DEVICE_BASE_NAME

Before the initial USB flash of each unit, open `src/main.cpp` and set:

```cpp
#define DEVICE_BASE_NAME "LoRa_XIAO1"   // change for each unit
```

Use a unique name for each board. The full device name — including MAC address — is printed to the serial monitor on every boot. Record this name and update the device table below.

---

## Flashing a Firmware Update

### Step 1 — Build the firmware first

In VS Code with PlatformIO, click the **Build** button (✓ checkmark in the bottom toolbar). Make sure it says `[SUCCESS]` before proceeding.

> ⚠️ Do not use the PlatformIO Upload button for OTA — it will close the serial monitor and attempt a USB flash. OTA uploads are done via Terminal only.

---

### Step 2 — Open two Terminal windows

**Terminal Window 1 — Serial Monitor (optional, requires USB connection):**

```bash
screen /dev/cu.usbmodem1101 115200
```

> To exit screen later: press `Ctrl+A` then `K` then `Y`

**Terminal Window 2 — OTA Upload:**

Navigate to the project folder:

```bash
cd /path/to/XIAO-LoRa-CO2-monitoring
```

---

### Step 3 — Trigger OTA mode from Node-RED

Select the target device in the **Select Sensor** dropdown on the Sensor Calibration dashboard page, then click **Trigger OTA Update Period**. This sends a LoRa command through the appropriate OMG gateway to the device.

Alternatively, the MQTT payload format is:

```json
{"message":"{\"target\":\"LoRa_XIAO1_2C02A7D4DB1C\",\"ota\":true}"}
```

Sent to MQTT topic (local gateway):
```
OMGhome/OMG_ESP32_LORA/commands/MQTTtoLORA
```

Or for Nevada devices:
```
MushLoRa/OMG_LORA_MOAPA/commands/MQTTtoLORA
```

> Only the targeted device will respond — others will ignore the command.

---

### Step 4 — Wait for OTA ready confirmation

The Node-RED OTA Status display on the dashboard will show:
```
OTA mode started on LoRa_XIAO1_2C02A7D4DB1C - 3 minute window open
```

If monitoring via USB serial:
```
✓ OTA mode command received
--- OTA Mode Triggered via LoRa ---
WiFi connected: 10.0.x.xxx
OTA ready - waiting 3 minutes...
```

You now have **3 minutes** to push the firmware.

---

### Step 5 — Run the OTA upload script

In Terminal Window 2, run the script for the target device:

```bash
./ota/ota_xiao1.sh
```

You should see:

```
Sending invitation to 10.0.x.xxx
Authenticating...OK
Uploading.......................................
```

The device reboots automatically after a successful upload and resumes normal LoRa operation within about 30 seconds.

---

## Device Reference

| Script | Device Name | Network | IP Address |
|--------|-------------|---------|------------|
| ota_xiao1.sh | LoRa_XIAO1_2C02A7D4DB1C | SSID1 (local) | 10.0.1.113 |
| ota_xiao_co2_1.sh | LoRa_XIAO_CO2_1_D893A8D4DB1C | SSID2 (remote) | TBD after deployment |
| ota_xiao_co2_2.sh | LoRa_XIAO_CO2_2_B893A8D4DB1C | SSID2 (remote) | TBD after deployment |
| ota_xiao_co2_3.sh | LoRa_XIAO_CO2_3_BC1BA7D4DB1C | SSID2 (remote) | TBD after deployment |

> Update IP addresses after deployment. Remote IPs are local network addresses on the SSID2 network, reachable via Tailscale subnet routing on the Nevada Raspberry Pi.

---

## Troubleshooting

**"Permission denied" running the script (Mac/Linux):**
```bash
chmod +x ota/ota_xiao1.sh
```

**"Host not found" or "Sending invitation failed":**
- The OTA window is not open — trigger from Node-RED first and wait for confirmation before running the script
- The IP address has changed — check your router or LanScan for the current IP and update the script
- You are not on the same network as the device
- For Nevada devices: confirm Tailscale is running and connected on your Mac and that the Nevada Pi subnet router is active

**"Authentication Failed":**
- The OTA password in the script does not match `OTA_PASSWORD` in `secrets.h`
- Rebuild and re-flash via USB with matching credentials

**Device appears offline in network scanner during OTA window:**
- This is normal — the device may not respond to pings while in the OTA loop. The window is still open. Proceed with the upload script.

**OTA window closed before upload completed:**
- Trigger again from Node-RED to open a new 3-minute window
- Always build firmware before triggering OTA to avoid delays

**Device does not respond to OTA command:**
- Confirm the device name in the dropdown matches exactly (case sensitive)
- The OMG gateway must be online and connected to MQTT for LoRa commands to reach the device

---

## Nevada / Tailscale Subnet Routing Notes

For remote Nevada devices:

1. Tailscale must be installed and running on your Mac
2. Tailscale must be installed and running on the Nevada Raspberry Pi, configured as a **subnet router** advertising the local K1W1 network
3. The Nevada XIAO units connect to the K1W1 WiFi network when OTA mode is triggered
4. Use the devices' normal local IP addresses in the OTA scripts — no special Tailscale IPs needed
5. Update the `ota/` scripts and device table above after initial deployment when IP addresses are known

---

*Last updated: 2026*