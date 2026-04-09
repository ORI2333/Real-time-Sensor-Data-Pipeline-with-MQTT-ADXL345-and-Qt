# Raspberry Pi 端代码（预留）

本目录用于后续放置树莓派采集/发布端代码。

建议后续结构：

- src/            树莓派端源码
- config/         设备参数与运行配置
- scripts/        部署与启动脚本
- requirements.txt 或 CMakeLists.txt

与桌面 Qt 工程约定：

- Broker 默认：127.0.0.1:1883（联调时按实际网络修改）
- Topic 命名建议：sensor/<device_id>/telemetry
- Payload 建议：JSON（包含 timestamp 与传感器字段）
