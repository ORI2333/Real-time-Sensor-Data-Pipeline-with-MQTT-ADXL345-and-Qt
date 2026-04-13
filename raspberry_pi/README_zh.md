# Raspberry Pi 端说明

本目录已包含树莓派端源码与可执行样例，主要用于采集 ADXL345 数据并通过 MQTT 发布。

完整技术文档请优先阅读：

- ../docs/项目技术说明与论文参考（中文版）.md

## 目录结构

- 程序源码/ADXL345.cpp、ADXL345.h：ADXL345 驱动与姿态角计算
- 程序源码/I2CDevice.cpp、I2CDevice.h：I2C 通信基础封装
- 程序源码/publisher.cpp：采集并发布 JSON 数据
- 程序源码/subscriber_led_wiringpi.cpp：订阅消息并联动 LED 示例
- 执行程序/publisher：发布端可执行样例
- 执行程序/subscriber_led：订阅端可执行样例

## 快速开始

1. 检查 I2C 与硬件连接

- 传感器地址默认 0x53
- 可用命令检查设备：`i2cdetect -y 1`

2. 编译 publisher（示例）

```bash
g++ -std=c++17 publisher.cpp ADXL345.cpp I2CDevice.cpp -ljsoncpp -lpaho-mqtt3c -o publisher
```

3. 运行 publisher

```bash
./publisher 1
```

说明：程序参数 0/1/2 对应 MQTT QoS。

## 主题与协议注意事项

- publisher.cpp 默认发布主题：sensor/adxl345
- subscriber_led_wiringpi.cpp 默认订阅主题：sensor/adxl345/PC

如果希望两者直接联动，请统一主题配置。
