//  LoRa XIAO SCD41 CO2 Sensor Node
//  ============================================================
//  Monitors CO2, temperature, and humidity using an Adafruit SCD41
//  sensor and transmits readings over LoRa to a LilyGo T3 LoRa32
//  running OpenMQTTGateway (OMG), which forwards data to a Mosquitto
//  MQTT broker on a Raspberry Pi. Node-RED is used for dashboards
//  and remote FRC recalibration commands sent back via LoRa.
//
//  Hardware:
//    - Seeed Studio XIAO ESP32S3 + Wio-SX1262 LoRa B2B Kit
//    - Adafruit SCD41 CO2/Temp/RH sensor (I2C, address 0x62)
//    - Adafruit MAX17048 LiPo fuel gauge (I2C, shared bus)
//    - 3.7V LiPo battery
//
//  Note: WiFi is intentionally not used in this firmware.
//  OTA updates over WiFi were attempted but could not be made
//  to work reliably alongside the LoRa radio on the ESP32S3.
//  Firmware updates require a USB-C cable connection.
//
//  Before flashing: set DEVICE_BASE_NAME below to a unique name
//  for each unit (e.g. "LoRa_XIAO1", "LoRa_XIAO2"). The full
//  6-byte MAC address is appended automatically as a suffix.
//  ============================================================

//#define DEVICE_BASE_NAME "LoRa_XIAO1"   // <-- CHANGE THIS for each unit
//#define DEVICE_BASE_NAME "LoRa_XIAO_CO2_1"
//#define DEVICE_BASE_NAME "LoRa_XIAO_CO2_2"
#define DEVICE_BASE_NAME "LoRa_XIAO_CO2_3"

#include <Arduino.h>
#include <Wire.h>
#include <SensirionI2cScd4x.h>
#include <ArduinoJson.h>
#include <RadioLib.h>
#include <SPI.h>
#include <Adafruit_MAX1704X.h>

// --- LoRa pins (Seeed XIAO ESP32S3 + Wio-SX1262 B2B Kit) ---
// Note: These pin numbers are specific to the B2B kit and differ
// from the standalone Wio-SX1262 board.
SX1262 radio = new Module(41, 39, 42, 40); // NSS, DIO1, RST, BUSY
#define LED_GPIO 48

// --- SCD41 CO2 sensor ---
SensirionI2cScd4x scd4x;

// --- MAX17048 battery monitor ---
Adafruit_MAX17048 maxlipo;
bool batteryAvailable = false;

// --- Measurement interval ---
// 9 seconds (not 10) to avoid packet collisions between two units
// transmitting on the same LoRa frequency
const unsigned long MEAS_INTERVAL_MS = 31000;
unsigned long lastRead = 0;

// --- Last-good readings (used if sensor read fails) ---
uint16_t lastCO2 = 0;
float lastTemp = 0.0;
float lastHum  = 0.0;
float lastBattVolt = 0.0;
float lastBattPercent = 0.0;
float lastBattCRate = 0.0;

// --- FRC status tracking ---
String lastFrcStatus = "none";  // "none", "received", "success", "failed", "no_persist"
unsigned long lastFrcTime = 0;
uint16_t lastFrcTarget = 0;     // Used to reject duplicate FRC commands within 30s

// --- LoRa RX diagnostics ---
unsigned long rxRestartCount = 0;
unsigned long packetsReceived = 0;
unsigned long crcErrors = 0;

// --- Battery warning thresholds ---
const float BATTERY_WARN_PERCENT = 20.0;
const float BATTERY_CRITICAL_PERCENT = 10.0;

// --- Device name (built from DEVICE_BASE_NAME + MAC address) ---
String device_name;

// --- LoRa TX/RX buffers ---
uint8_t txBuffer[256];
uint8_t rxBuffer[256];

// --- Interrupt-driven RX/TX flags ---
volatile bool receivedFlag = false;
volatile bool transmittedFlag = false;
volatile bool isTransmitting = false;

// --- Interrupt handlers ---
#if defined(ESP8266) || defined(ESP32)
    ICACHE_RAM_ATTR
#endif
void setRxFlag(void) {
    receivedFlag = true;
}

#if defined(ESP8266) || defined(ESP32)
    ICACHE_RAM_ATTR
#endif
void setTxFlag(void) {
    transmittedFlag = true;
    isTransmitting = false;
}

/* ================= BATTERY FUNCTIONS ================= */

bool readBattery(float &voltage, float &percent, float &crate) {
    if (!batteryAvailable) {
        return false;
    }

    voltage = maxlipo.cellVoltage();
    percent = maxlipo.cellPercent();
    crate = maxlipo.chargeRate();

    // NaN can occur with a fully charged battery
    if (isnan(voltage) || isnan(percent) || isnan(crate)) {
        Serial.println("NaN detected in battery reading");
        return false;
    }

    // Sanity check valid ranges (allow slightly over 100% as MAX17048 can report 104%)
    if (voltage < 2.5 || voltage > 4.5 || percent < 0 || percent > 110) {
        return false;
    }

    return true;
}

String getBatteryStatus(float percent, bool validReading) {
    if (!validReading || !batteryAvailable) {
        return "UNKNOWN";
    }
    if (percent <= BATTERY_CRITICAL_PERCENT) {
        return "CRITICAL";
    } else if (percent <= BATTERY_WARN_PERCENT) {
        return "WARNING";
    } else {
        return "OK";
    }
}

/* ================= SENSOR FUNCTIONS ================= */

bool readSensor(uint16_t &co2, float &temperature, float &humidity) {
    for (int attempt = 0; attempt < 3; attempt++) {
        int16_t ret = scd4x.readMeasurement(co2, temperature, humidity);
        if (ret == 0) return true;
        delay(50);
    }
    return false;
}

void disableABC() {
    // ASC (Automatic Self-Calibration) assumes the sensor regularly sees
    // outdoor air (~420 ppm). This is not the case in a mushroom grow room,
    // so ASC must be disabled and persisted to EEPROM to prevent drift.
    scd4x.stopPeriodicMeasurement();
    delay(500);

    int16_t ret = scd4x.setAutomaticSelfCalibrationEnabled(0);
    if (ret == 0) {
        Serial.println("ABC disabled successfully");
        ret = scd4x.persistSettings();
        if (ret == 0) {
            Serial.println("ABC disable setting saved to EEPROM");
        } else {
            Serial.printf("Failed to persist ABC setting, error: %d\n", ret);
        }
    } else {
        Serial.printf("Failed to disable ABC, error: %d\n", ret);
    }

    delay(500);
    scd4x.startPeriodicMeasurement();
}

/* ================= TEMPERATURE OFFSET CALIBRATION ================= */

void setTemperatureOffset(float offsetDegC) {
    Serial.printf("Setting temperature offset to %.2f °C\n", offsetDegC);

    scd4x.stopPeriodicMeasurement();
    delay(500);

    int16_t ret = scd4x.setTemperatureOffset(offsetDegC);

    bool success = false;
    if (ret == 0) {
        Serial.printf("Temperature offset set to %.2f °C\n", offsetDegC);
        ret = scd4x.persistSettings();
        if (ret == 0) {
            Serial.println("Temperature offset saved to EEPROM");
            success = true;
        } else {
            Serial.printf("Failed to persist temperature offset, error: %d\n", ret);
        }
    } else {
        Serial.printf("Failed to set temperature offset, error: %d\n", ret);
    }

    delay(500);
    scd4x.startPeriodicMeasurement();

    StaticJsonDocument<256> doc;
    doc["value"] = device_name;
    doc["temp_offset_set"] = success;
    doc["temp_offset_value"] = offsetDegC;
    doc["co2"] = lastCO2;
    doc["temp"] = lastTemp;
    doc["hum"] = lastHum;
    doc["batt_volt"] = lastBattVolt;
    doc["batt_percent"] = lastBattPercent;
    doc["batt_crate"] = lastBattCRate;

    size_t n = serializeJson(doc, txBuffer);
    digitalWrite(LED_GPIO, HIGH);
    radio.transmit(txBuffer, n);
    digitalWrite(LED_GPIO, LOW);

    radio.startReceive();
    receivedFlag = false;
}

void getTemperatureOffset() {
    Serial.println("Reading current temperature offset...");

    scd4x.stopPeriodicMeasurement();
    delay(500);

    float currentOffset = 0.0;
    int16_t ret = scd4x.getTemperatureOffset(currentOffset);

    if (ret == 0) {
        Serial.printf("Current temperature offset: %.2f °C\n", currentOffset);
    } else {
        Serial.printf("Failed to read temperature offset, error: %d\n", ret);
    }

    scd4x.startPeriodicMeasurement();

    StaticJsonDocument<256> doc;
    doc["value"] = device_name;
    doc["temp_offset_current"] = currentOffset;
    doc["temp_offset_read_success"] = (ret == 0);
    doc["co2"] = lastCO2;
    doc["temp"] = lastTemp;
    doc["hum"] = lastHum;
    doc["batt_volt"] = lastBattVolt;
    doc["batt_percent"] = lastBattPercent;
    doc["batt_crate"] = lastBattCRate;

    size_t n = serializeJson(doc, txBuffer);
    digitalWrite(LED_GPIO, HIGH);
    radio.transmit(txBuffer, n);
    digitalWrite(LED_GPIO, LOW);

    radio.startReceive();
    receivedFlag = false;
}

/* ================= ALTITUDE CALIBRATION ================= */

void setSensorAltitude(uint16_t altitudeMeters) {
    Serial.printf("Setting sensor altitude to %u meters\n", altitudeMeters);

    if (altitudeMeters > 3000) {
        Serial.println("Error: Altitude must be 0-3000 meters per SCD41 datasheet");
        return;
    }

    scd4x.stopPeriodicMeasurement();
    delay(500);

    int16_t ret = scd4x.setSensorAltitude(altitudeMeters);

    bool success = false;
    if (ret == 0) {
        Serial.printf("Altitude set to %u m\n", altitudeMeters);
        ret = scd4x.persistSettings();
        if (ret == 0) {
            Serial.println("Altitude saved to EEPROM");
            success = true;
        } else {
            Serial.printf("Failed to persist altitude, error: %d\n", ret);
        }
    } else {
        Serial.printf("Failed to set altitude, error: %d\n", ret);
    }

    delay(500);
    scd4x.startPeriodicMeasurement();

    StaticJsonDocument<256> doc;
    doc["value"] = device_name;
    doc["altitude_set"] = success;
    doc["altitude_value"] = altitudeMeters;
    doc["co2"] = lastCO2;
    doc["temp"] = lastTemp;
    doc["hum"] = lastHum;
    doc["batt_volt"] = lastBattVolt;
    doc["batt_percent"] = lastBattPercent;
    doc["batt_crate"] = lastBattCRate;

    size_t n = serializeJson(doc, txBuffer);
    digitalWrite(LED_GPIO, HIGH);
    radio.transmit(txBuffer, n);
    digitalWrite(LED_GPIO, LOW);

    radio.startReceive();
    receivedFlag = false;
}

void getSensorAltitude() {
    Serial.println("Reading current sensor altitude...");

    scd4x.stopPeriodicMeasurement();
    delay(500);

    uint16_t currentAltitude = 0;
    int16_t ret = scd4x.getSensorAltitude(currentAltitude);

    if (ret == 0) {
        Serial.printf("Current altitude: %u meters\n", currentAltitude);
    } else {
        Serial.printf("Failed to read altitude, error: %d\n", ret);
    }

    scd4x.startPeriodicMeasurement();

    StaticJsonDocument<256> doc;
    doc["value"] = device_name;
    doc["altitude_current"] = currentAltitude;
    doc["altitude_read_success"] = (ret == 0);
    doc["co2"] = lastCO2;
    doc["temp"] = lastTemp;
    doc["hum"] = lastHum;
    doc["batt_volt"] = lastBattVolt;
    doc["batt_percent"] = lastBattPercent;
    doc["batt_crate"] = lastBattCRate;

    size_t n = serializeJson(doc, txBuffer);
    digitalWrite(LED_GPIO, HIGH);
    radio.transmit(txBuffer, n);
    digitalWrite(LED_GPIO, LOW);

    radio.startReceive();
    receivedFlag = false;
}

/* ================= FORCED RECALIBRATION (FRC) ================= */

void forceSCD41Calibration(uint16_t targetCO2) {
    Serial.println("Starting Forced Recalibration...");

    uint16_t co2 = lastCO2;
    if (co2 == 0) {
        Serial.println("No valid CO2 reading available, skipping FRC");
        lastFrcStatus = "no_co2_data";
        lastFrcTime = millis();
        return;
    }

    Serial.printf("Device: %s, Current CO2: %u ppm, FRC target: %u ppm\n",
        device_name.c_str(), co2, targetCO2);

    // Brief stability wait — sensor should already be equilibrated outdoors
    // before the FRC command is sent from Node-RED
    Serial.println("Waiting 5s for sensor stability...");
    delay(5000);

    scd4x.stopPeriodicMeasurement();
    delay(500);

    uint16_t frcCorrection = 0;
    int16_t ret = scd4x.performForcedRecalibration(targetCO2, frcCorrection);

    bool frcSuccess = false;
    if (ret == 0) {
        Serial.printf("FRC success - correction value: %u\n", frcCorrection);
        ret = scd4x.persistSettings();
        if (ret == 0) {
            Serial.println("FRC calibration saved to EEPROM");
            frcSuccess = true;
            lastFrcStatus = "success";
        } else {
            Serial.printf("Failed to persist FRC, error: %d\n", ret);
            lastFrcStatus = "no_persist";
        }
    } else {
        Serial.printf("FRC FAILED - code: %d, correction: %u\n", ret, frcCorrection);
        lastFrcStatus = "failed";
    }

    lastFrcTime = millis();

    delay(500);
    scd4x.startPeriodicMeasurement();
    Serial.println("Periodic measurement restarted");

    StaticJsonDocument<256> doc;
    doc["value"]          = device_name;
    doc["co2"]            = lastCO2;
    doc["temp"]           = lastTemp;
    doc["hum"]            = lastHum;
    doc["frc_applied"]    = frcSuccess;
    doc["frc_correction"] = frcCorrection;
    doc["frc_persisted"]  = frcSuccess;
    doc["batt_volt"]      = lastBattVolt;
    doc["batt_percent"]   = lastBattPercent;
    doc["batt_crate"]     = lastBattCRate;

    size_t n = serializeJson(doc, txBuffer);
    digitalWrite(LED_GPIO, HIGH);
    int state = radio.transmit(txBuffer, n);
    digitalWrite(LED_GPIO, LOW);

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("FRC result transmitted via LoRa");
    } else {
        Serial.printf("FRC result TX failed, code: %d\n", state);
    }

    radio.standby();
    delay(10);

    int rxState = radio.startReceive();
    if (rxState != RADIOLIB_ERR_NONE) {
        Serial.printf("FRC: RX restart failed, code: %d\n", rxState);
    }
    receivedFlag = false;
}

/* ================= LORA COMMAND HANDLER ================= */

void handleLoRaCommand() {
    if (!receivedFlag) return;

    receivedFlag = false;
    packetsReceived++;

    int state = radio.readData(rxBuffer, sizeof(rxBuffer));

    if (state == RADIOLIB_ERR_NONE) {
        size_t received = radio.getPacketLength();

        if (received > 0 && received < sizeof(rxBuffer)) {
            rxBuffer[received] = '\0';
            Serial.print("LoRa RX RAW: ");
            Serial.println((char*)rxBuffer);

            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, rxBuffer);

            if (error) {
                Serial.print("JSON parse error: ");
                Serial.println(error.c_str());
                radio.startReceive();
                return;
            }

            // Commands may arrive wrapped in a "message" field (OMG format)
            // or unwrapped (direct JSON). Handle both.
            StaticJsonDocument<256> cmd;
            bool isCommand = false;

            if (doc.containsKey("message")) {
                const char* innerJson = doc["message"];
                if (innerJson && deserializeJson(cmd, innerJson) == DeserializationError::Ok) {
                    isCommand = true;
                }
            } else if (doc.containsKey("target")) {
                cmd = doc;
                isCommand = true;
            }

            if (!isCommand || !cmd.containsKey("target")) {
                // Not a command — probably telemetry from the other sensor unit
                radio.startReceive();
                return;
            }

            String target = cmd["target"].as<String>();
            Serial.printf("Command target: %s, My name: %s\n",
                target.c_str(), device_name.c_str());

            if (target != device_name) {
                Serial.println("Not for me, ignoring");
                radio.startReceive();
                return;
            }

            digitalWrite(LED_GPIO, HIGH);

            if (cmd.containsKey("frc")) {
                uint16_t targetCO2 = cmd["frc"];
                Serial.printf("✓ FRC command received! Target: %u ppm\n", targetCO2);

                // Reject duplicate FRC commands within 30 seconds
                // (Node-RED sends 3-4 times for reliability)
                if (targetCO2 == lastFrcTarget && (millis() - lastFrcTime) < 30000) {
                    Serial.println("Duplicate FRC ignored (same target within 30s)");
                    digitalWrite(LED_GPIO, LOW);
                    radio.startReceive();
                    return;
                }

                lastFrcStatus = "received";
                lastFrcTime = millis();
                lastFrcTarget = targetCO2;

                forceSCD41Calibration(targetCO2);
                digitalWrite(LED_GPIO, LOW);
                return;
            }

            if (cmd.containsKey("temp_offset")) {
                float offset = cmd["temp_offset"];
                Serial.printf("✓ Temperature offset command: %.2f °C\n", offset);
                setTemperatureOffset(offset);
                digitalWrite(LED_GPIO, LOW);
                return;
            }

            if (cmd.containsKey("get_temp_offset")) {
                Serial.println("✓ Get temperature offset command received");
                getTemperatureOffset();
                digitalWrite(LED_GPIO, LOW);
                return;
            }

            if (cmd.containsKey("altitude")) {
                uint16_t altitude = cmd["altitude"];
                Serial.printf("✓ Altitude command: %u meters\n", altitude);
                setSensorAltitude(altitude);
                digitalWrite(LED_GPIO, LOW);
                return;
            }

            if (cmd.containsKey("get_altitude")) {
                Serial.println("✓ Get altitude command received");
                getSensorAltitude();
                digitalWrite(LED_GPIO, LOW);
                return;
            }

            digitalWrite(LED_GPIO, LOW);

        }
    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        Serial.println("CRC error!");
        crcErrors++;
    }

    radio.startReceive();
}

/* ================= I2C SCANNER ================= */

void scanI2C() {
    Serial.println("Scanning I2C bus...");
    byte error, address;
    int nDevices = 0;

    for (address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if (error == 0) {
            Serial.printf("I2C device found at 0x%02X\n", address);
            nDevices++;
        }
    }

    if (nDevices == 0) {
        Serial.println("No I2C devices found!");
    } else {
        Serial.printf("Found %d device(s)\n", nDevices);
    }
    Serial.println("Scan complete");
}

/* ================= SETUP ================= */

void setup() {
    Serial.begin(115200);
    delay(2000);

    // Lock CPU to 240MHz — prevents automatic frequency scaling and light sleep
    // which were found to interfere with RadioLib LoRa interrupt handling
    // on battery power.
    setCpuFrequencyMhz(240);
    Serial.println("CPU locked to 240MHz");

    // Build unique device name from DEVICE_BASE_NAME + full 6-byte MAC address.
    // Using the full MAC (not just last 3 bytes) avoids duplicate names on
    // boards from the same manufacturing batch.
    uint64_t mac = ESP.getEfuseMac();
    char macStr[13];
    sprintf(macStr, "%02X%02X%02X%02X%02X%02X",
            (uint8_t)(mac >> 40),
            (uint8_t)(mac >> 32),
            (uint8_t)(mac >> 24),
            (uint8_t)(mac >> 16),
            (uint8_t)(mac >> 8),
            (uint8_t)mac);
    device_name = String(DEVICE_BASE_NAME) + "_" + String(macStr);
    Serial.print("Device name: ");
    Serial.println(device_name);

    Wire.begin();
    Wire.setClock(100000);
    delay(100);
    scanI2C();

    // Initialize MAX17048 battery monitor
    if (!maxlipo.begin()) {
        Serial.println("⚠️ MAX17048 not found - battery monitoring disabled");
        batteryAvailable = false;
    } else {
        Serial.println("✓ MAX17048 initialized");
        batteryAvailable = true;
        maxlipo.quickStart();
        delay(500);

        float testVolt = maxlipo.cellVoltage();
        float testPercent = maxlipo.cellPercent();
        float testCRate = maxlipo.chargeRate();
        Serial.printf("Battery: %.2fV, %.1f%%, C-Rate: %.2f%%/hr\n",
                     testVolt, testPercent, testCRate);

        if (testVolt < 2.5 || testVolt > 4.5) {
            Serial.println("⚠️ Battery readings invalid - disabling monitoring");
            batteryAvailable = false;
        }
    }

    // Initialize SCD41
    scd4x.begin(Wire, 0x62);
    scd4x.startPeriodicMeasurement();
    Serial.println("SCD41 initialized. Waiting 20s for first valid reading...");
    delay(20000);

    disableABC();

    // Verify ABC was successfully disabled
    delay(500);
    scd4x.stopPeriodicMeasurement();
    delay(500);
    uint16_t ascEnabled;
    int16_t ret = scd4x.getAutomaticSelfCalibrationEnabled(ascEnabled);
    if (ret == 0) {
        Serial.printf("ABC status confirmed: %s\n", ascEnabled ? "ENABLED ⚠️" : "DISABLED ✓");
    } else {
        Serial.printf("Failed to read ABC status, error: %d\n", ret);
    }

    scd4x.startPeriodicMeasurement();
    Serial.println("Waiting 5s before first transmission...");
    delay(5000);

    pinMode(LED_GPIO, OUTPUT);
    digitalWrite(LED_GPIO, LOW);

    // Initialize LoRa radio
    Serial.print("Initializing LoRa... ");
    int state = radio.begin();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("❌ LoRa init FAILED, code: %d\n", state);
        while (true);
    }

    // These settings must match the OpenMQTTGateway configuration exactly
    radio.setFrequency(915.0);
    radio.setOutputPower(14);
    radio.setSpreadingFactor(7);
    radio.setBandwidth(125.0);
    radio.setCodingRate(5);
    radio.setPreambleLength(8);
    radio.setSyncWord(0x12);
    radio.setCRC(true);

    radio.setDio1Action(setRxFlag);

    state = radio.startReceive();
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("✓ LoRa RX mode active");
    } else {
        Serial.printf("Start RX failed, code: %d\n", state);
    }

    Serial.println("Setup complete — listening for commands...");
}

/* ================= LOOP ================= */

void loop() {
    // Check for incoming LoRa commands (interrupt-driven, non-blocking)
    handleLoRaCommand();

    // Periodic sensor read and LoRa transmission
    if (millis() - lastRead >= MEAS_INTERVAL_MS && !isTransmitting) {
        lastRead = millis();

        uint16_t co2;
        float temperature, humidity;
        float battVolt = 0.0, battPercent = 0.0, battCRate = 0.0;
        bool battValid = false;

        // Read SCD41
        if (!readSensor(co2, temperature, humidity)) {
            co2 = lastCO2;
            temperature = lastTemp;
            humidity = lastHum;
            Serial.println("Measurement not ready, using last-good values");
        } else {
            lastCO2 = co2;
            lastTemp = temperature;
            lastHum = humidity;
        }

        Serial.printf("--- %s ---\n", device_name.c_str());
        Serial.printf("CO2: %u ppm  Temp: %.2f °C  Hum: %.1f%%\n",
                     co2, temperature, humidity);

        // Read MAX17048
        if (batteryAvailable) {
            if (readBattery(battVolt, battPercent, battCRate)) {
                lastBattVolt = battVolt;
                lastBattPercent = battPercent;
                lastBattCRate = battCRate;
                battValid = true;
                Serial.printf("Battery: %.2fV  %.1f%%  C-Rate: %.2f%%/hr\n",
                             battVolt, battPercent, battCRate);
                if (battPercent <= BATTERY_CRITICAL_PERCENT) {
                    Serial.printf("🔴 CRITICAL BATTERY: %.1f%%\n", battPercent);
                } else if (battPercent <= BATTERY_WARN_PERCENT) {
                    Serial.printf("⚠️ LOW BATTERY: %.1f%%\n", battPercent);
                }
            } else {
                Serial.println("Battery read failed");
            }
        }

        String battStatus = getBatteryStatus(battPercent, battValid);

        // Build JSON payload
        StaticJsonDocument<256> doc;
        doc["value"]   = device_name;
        doc["co2"]     = co2;
        doc["temp"]    = temperature;
        doc["hum"]     = humidity;
        doc["rssi"]    = radio.getRSSI();
        doc["snr"]     = radio.getSNR();
        doc["pferror"] = 0;
        doc["batt_volt"]    = battValid ? battVolt    : 0.0;
        doc["batt_percent"] = battValid ? battPercent : 0.0;
        doc["batt_crate"]   = battValid ? battCRate   : 0.0;
        doc["batt_status"]  = battStatus;

        // Include FRC status for 60 seconds after a calibration event
        if (lastFrcStatus != "none" && (millis() - lastFrcTime) < 60000) {
            doc["last_frc_status"]  = lastFrcStatus;
            doc["last_frc_age_sec"] = (millis() - lastFrcTime) / 1000;
        }

        // Include CRC error count if non-zero (useful for diagnosing LoRa issues)
        if (crcErrors > 0) {
            doc["crc_errors"] = crcErrors;
        }

        size_t n = serializeJson(doc, txBuffer);

        // Check for any commands that arrived during the sensor read
        handleLoRaCommand();
        delay(50);
        handleLoRaCommand();

        // Async transmit — allows RX to continue while TX is in progress
        digitalWrite(LED_GPIO, HIGH);
        transmittedFlag = false;
        isTransmitting = true;
        radio.setDio1Action(setTxFlag);

        int state = radio.startTransmit(txBuffer, n);
        if (state != RADIOLIB_ERR_NONE) {
            Serial.printf("TX start failed, code: %d\n", state);
            isTransmitting = false;
            digitalWrite(LED_GPIO, LOW);
            radio.setDio1Action(setRxFlag);
            radio.startReceive();
        } else {
            Serial.println("TX started (async)...");
        }
    }

    // Handle async TX completion
    if (transmittedFlag) {
        transmittedFlag = false;
        digitalWrite(LED_GPIO, LOW);
        Serial.println("LoRa TX complete");

        radio.setDio1Action(setRxFlag);
        delay(10);
        int rxState = radio.startReceive();
        rxRestartCount++;
        if (rxState != RADIOLIB_ERR_NONE) {
            Serial.printf("RX restart failed, code: %d\n", rxState);
        }
        receivedFlag = false;
    }
}
