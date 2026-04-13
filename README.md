# Real-time Sensor Data Pipeline with MQTT, ADXL345, and Qt

## 1. Overview

This project implements an end-to-end real-time sensing pipeline using Raspberry Pi 5, an ADXL345 accelerometer, MQTT (Mosquitto + Paho C), and a Qt desktop client.

Main capabilities:

- Read ADXL345 acceleration data over I2C
- Compute orientation values (pitch/roll)
- Collect Raspberry Pi CPU load and temperature
- Publish JSON payloads through MQTT
- Visualize live data and rolling statistics in Qt
- Drive local LEDs based on tilt level

## 2. System Components

1. Raspberry Pi acquisition node
- Reads ADXL345 raw data via I2C
- Calculates pitch/roll
- Collects CPU metrics

2. MQTT transport layer
- Mosquitto broker
- Paho MQTT C client for publish/subscribe

3. Qt desktop application
- Connect/subscribe/publish over MQTT
- Parse JSON payloads
- Draw real-time trends and logs
- Show 60-second rolling statistics

4. LED actuator (Raspberry Pi)
- Uses tilt thresholds to switch LED states

## 3. Repository Structure

```text
.
├─ src/                    # Qt source files
├─ include/                # Qt headers
├─ ui/                     # Qt Designer files
├─ tools/                  # build/run/package scripts
├─ raspberry_pi/           # Raspberry Pi sources
│  ├─ 程序源码/
│  │  ├─ I2CDevice.h/.cpp
│  │  ├─ ADXL345.h/.cpp
│  │  ├─ publisher.cpp
│  │  └─ subscriber_led_wiringpi.cpp
├─ EEN1071Ass2.pro         # qmake project file
└─ README_zh.md            # Chinese backup of this document
```

## 4. Data Format

Example payload published by the Raspberry Pi node:

```json
{
  "pitch": -31.87,
  "roll": -52.19,
  "accel_x": -129,
  "accel_y": -193,
  "accel_z": -76,
  "cpu_load": 0.12,
  "cpu_temp_c": 51.8,
  "timestamp": "2026-04-10T07:17:46Z"
}
```

## 5. Quick Start

1. Start Mosquitto broker (local machine or LAN host).
2. Build and run the Qt app.
3. Run Raspberry Pi `publisher` with QoS argument (0/1/2).
4. Observe logs, charts, and rolling statistics in Qt.
5. Optionally run `subscriber_led_wiringpi` for local LED demonstration.

## 6. Notes

- Use `raspberry_pi/程序源码/` as the authoritative Raspberry Pi source directory.
- MQTT topics and credentials are currently defined in source macros and can be adjusted for your network.
- Chinese source copies are preserved as `_zh.md` files where requested.
