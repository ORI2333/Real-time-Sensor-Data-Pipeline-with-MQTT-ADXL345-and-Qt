# 基于 MQTT、ADXL345 与 Qt 的实时传感器数据采集与可视化系统

## 1. 项目简介

本项目实现了一套基于 Raspberry Pi 5、ADXL345 三轴加速度传感器、Mosquitto Broker 和 Qt 图形界面的实时传感器数据采集与显示系统。系统通过 I?C 总线完成树莓派与 ADXL345 之间的数据通信，通过 MQTT 协议实现设备与上位机之间的数据传输，并通过 Qt 上位机完成消息订阅、JSON 解析、实时曲线绘制、运行日志显示以及最近 60 秒统计信息展示。

除了数据采集与显示功能外，系统还实现了基于姿态角的 LED 状态联动，可根据传感器倾斜程度点亮不同 LED，形成“采集-传输-显示-执行”的完整闭环。该项目适合作为物联网课程设计、嵌入式系统实验、MQTT 综合应用项目或论文原型系统。

---

## 2. 系统组成

本系统由四个部分组成：

1. Raspberry Pi 5 采集节点  
   通过 I?C 接口连接 ADXL345，读取三轴加速度数据，并计算 pitch 和 roll，同时读取树莓派 CPU 温度与 CPU 负载。

2. MQTT 通信层  
   采用 Mosquitto 作为 Broker，采用 Eclipse Paho MQTT C 客户端库实现消息发布与订阅。

3. Qt 图形化上位机  
   负责连接 Broker、订阅主题、解析 JSON 数据、显示运行日志、绘制实时图形，并统计最近 60 秒内的数据平均值、最小值和最大值。

4. LED 联动执行器  
   在树莓派端根据传感器当前倾斜程度控制三个 LED，分别表示低、中、高三种状态。

---

## 3. 项目特点

本项目具有以下特点：

- 实现了从传感器采集到桌面可视化的完整数据链路
- 使用 JSON 作为统一数据负载格式，便于扩展与调试
- 支持 MQTT 的 QoS 0、QoS 1 和 QoS 2 测试
- 支持遗嘱消息和保留消息机制
- Qt 上位机既可以订阅显示，也可以主动发布 JSON 消息
- 支持实时曲线与 60 秒滚动统计
- 支持本地 LED 联动演示，具备较好的展示效果

---

## 4. 仓库结构

当前仓库主要包含 Qt 上位机工程和树莓派终端源码两部分。

```text
.
├─ src/                    # Qt 源码
├─ include/                # Qt 头文件
├─ ui/                     # Qt Designer 界面文件
├─ tools/                  # 构建、运行、打包脚本
├─ raspberry_pi/           # 树莓派终端相关代码
│  ├─ 程序源码/
│  │  ├─ I2CDevice.h/.cpp
│  │  ├─ ADXL345.h/.cpp
│  │  ├─ publisher.cpp
│  │  └─ subscriber_led_wiringpi.cpp
├─ EEN1071Ass2.pro         # Qt 工程文件
├─ README_backup.md        # 原始说明文档备份
└─ readme.md               # 中文说明文档（本文件）
```

说明：请优先以 `raspberry_pi/程序源码/` 下的源码为准。

## 5. 系统工作原理

系统运行时，Raspberry Pi 5 首先通过 I?C 读取 ADXL345 的寄存器数据，获得三轴加速度值。随后根据加速度分量计算姿态角 pitch 和 roll，并同步读取树莓派自身的 CPU 温度和 CPU 负载。采集到的数据会被封装为 JSON 格式，通过 MQTT 发布到指定主题。

Qt 上位机连接到同一 Broker 后，订阅对应主题，接收到 JSON 消息后进行解析，并将选定指标以实时曲线形式显示在界面中，同时在下方显示最近 60 秒内的平均值、最小值和最大值。若设备断开连接，则通过遗嘱消息或状态消息更新界面中的设备状态。与此同时，树莓派本地还会根据姿态角控制三个 LED，实现低、中、高三种等级的状态联动。

## 6. 数据格式

树莓派终端发布的典型 JSON 数据格式如下：

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

字段含义：

- `pitch`：俯仰角
- `roll`：横滚角
- `accel_x` / `accel_y` / `accel_z`：三轴加速度原始数据
- `cpu_load`：树莓派 CPU 负载
- `cpu_temp_c`：树莓派 CPU 温度（摄氏度）
- `timestamp`：UTC 时间戳

Qt 上位机支持对这些字段进行解析和显示，并兼容部分简化字段命名。

## 7. 主要功能

### 7.1 树莓派终端

树莓派终端主要负责：

- 通过 I?C 读取 ADXL345 数据
- 计算 pitch 和 roll
- 读取 CPU 温度与 CPU 负载
- 构造 JSON 数据并通过 MQTT 发布
- 支持不同 QoS 等级
- 发布在线/离线状态消息
- 控制 LED 联动显示

### 7.2 Qt 上位机

Qt 上位机主要负责：

- 连接 MQTT Broker
- 订阅数据主题与遗嘱主题
- 解析 JSON 数据
- 显示实时曲线
- 显示运行日志
- 计算最近 60 秒统计值
- 主动发布 JSON 测试消息
- 显示设备在线/离线状态

## 8. 软件环境

### 8.1 Windows 上位机环境

推荐环境如下：

- Windows 10 / 11（x64）
- Qt 5.14.2
- MinGW 7.3 64-bit
- qmake
- mingw32-make
- Eclipse Paho MQTT C
- Mosquitto
- VS Code（可选）

Qt 工程文件为：`EEN1071Ass2.pro`

### 8.2 Raspberry Pi 终端环境

推荐环境如下：

- Raspberry Pi 5
- Linux 系统
- GCC / G++
- JsonCpp
- Eclipse Paho MQTT C
- WiringPi
- I?C 相关头文件与驱动支持

## 9. 快速使用流程

建议按以下顺序进行部署与调试：

1. 启动 MQTT Broker  
   先在 Windows 或树莓派端启动 Mosquitto Broker，并确认 Broker 地址、端口号、用户名和密码配置正确。

2. 运行 Qt 上位机  
   打开 Qt 工程，编译并运行程序，或者直接使用已打包的可执行文件。填写 Broker 地址、客户端 ID、用户名、密码和订阅主题后点击连接。

3. 运行树莓派采集发布程序  
   在树莓派上编译并运行 `publisher.cpp` 对应程序，指定 QoS 等级，例如 0、1 或 2。程序启动后会周期性读取传感器并发布 JSON 数据。

4. 观察图形与日志  
   Qt 上位机接收到消息后，会更新运行日志、设备状态、实时曲线以及最近 60 秒统计值。

5. 运行 LED 联动程序  
   在树莓派上运行 LED 联动程序后，改变 ADXL345 的姿态，即可观察到不同 LED 状态切换。

## 10. 适用场景

本项目适用于以下场景：

- MQTT 协议课程实验
- 嵌入式 Linux 课程设计
- 物联网综合项目
- Qt 上位机开发练习
- 传感器数据可视化展示
- 毕设或课程论文中的原型系统

## 11. 可改进方向

当前系统已经可以完成完整功能演示，但仍可从以下方向继续扩展：

- 增加更多传感器接入
- 增加多设备管理功能
- 扩展多主题订阅与显示
- 优化树莓派端自动编译和部署脚本
- 将 LED 联动进一步改为纯消息驱动模式
- 增加数据库存储或历史回放功能

## 12. 参考源码位置

### Qt 上位机核心代码

- `src/main.cpp`
- `include/mainwindow.h`
- `src/mainwindow.cpp`
- `EEN1071Ass2.pro`

### 树莓派终端核心代码

- `raspberry_pi/程序源码/I2CDevice.h`
- `raspberry_pi/程序源码/I2CDevice.cpp`
- `raspberry_pi/程序源码/ADXL345.h`
- `raspberry_pi/程序源码/ADXL345.cpp`
- `raspberry_pi/程序源码/publisher.cpp`
- `raspberry_pi/程序源码/subscriber_led_wiringpi.cpp`

