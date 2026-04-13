# Raspberry Pi Notes

This folder contains Raspberry Pi source code and executable samples for ADXL345 data acquisition and MQTT publishing.

Please refer to the main technical documentation first:

- ../docs/系统说明文档.md

## Directory Layout

- 程序源码/ADXL345.cpp, ADXL345.h: ADXL345 driver and orientation calculation
- 程序源码/I2CDevice.cpp, I2CDevice.h: low-level I2C access wrapper
- 程序源码/publisher.cpp: sensor acquisition and MQTT JSON publisher
- 程序源码/subscriber_led_wiringpi.cpp: subscriber + LED linkage demo
- 执行程序/publisher: compiled publisher sample
- 执行程序/subscriber_led: compiled subscriber sample

## Quick Start

1. Verify I2C and hardware wiring
- Default sensor address: `0x53`
- Check command: `i2cdetect -y 1`

2. Build publisher (example)

```bash
g++ -std=c++17 publisher.cpp ADXL345.cpp I2CDevice.cpp -ljsoncpp -lpaho-mqtt3c -o publisher
```

3. Run publisher

```bash
./publisher 1
```

Argument `0/1/2` maps to MQTT QoS level.

## Topic Notes

- Default publish topic in `publisher.cpp`: `sensor/adxl345`
- Default subscribe topic in `subscriber_led_wiringpi.cpp`: `sensor/adxl345/PC`

If you want direct linkage between them, align both topics.

Chinese backup is preserved as `raspberry_pi/README_zh.md`.
