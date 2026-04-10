# MQTT ADXL345 实时监控面板（Qt）

本项目是一个基于 Qt Widgets + MQTT（Paho C）的本地上位机，用于接收 JSON 传感器数据并进行实时曲线显示与 60 秒滚动统计。

## 依赖环境与软件包

### 1. 操作系统

- Windows 10/11（x64）

### 2. Qt 与编译工具链

- Qt 5.14.2（MinGW 7.3 64-bit）
- qmake（来自 Qt 5.14.2）
- mingw32-make（来自 Qt Tools）
- gdb（用于 VS Code 调试）

当前工程默认使用下列路径（如你本机不同，请同步修改脚本和 VS Code 配置）：

- `E:/Qt/5.14.2/mingw73_64/bin/qmake.exe`
- `E:/Qt/Tools/mingw730_64/bin/mingw32-make.exe`
- `E:/Qt/Tools/mingw730_64/bin/gdb.exe`

### 3. MQTT 相关依赖

- Eclipse Paho MQTT C（客户端库）
- Mosquitto（本地 Broker，默认 1883）
- MQTTX（联调发布工具，可选）

### 4. 开发工具（推荐）

- VS Code
- C/C++ 扩展（ms-vscode.cpptools）
- PowerShell 7 或 Windows PowerShell（用于执行 `tools/*.ps1`）

### 5. 快速环境自检

```powershell
# Qt 工具链
& "E:\Qt\5.14.2\mingw73_64\bin\qmake.exe" -v
& "E:\Qt\Tools\mingw730_64\bin\mingw32-make.exe" -v

# Broker 监听
Get-NetTCPConnection -LocalPort 1883 -State Listen

# 本工程构建
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tools\build_qt.ps1
```

## 环境保护与注意事项

- 编码保护：工程已按 UTF-8 配置，源码文件请保持 UTF-8（避免中文乱码）。
- 路径保护：Qt 路径变更后，需要同步更新 [tools/build_qt.ps1](tools/build_qt.ps1)、[tools/run_qt.ps1](tools/run_qt.ps1)、[.vscode/launch.json](.vscode/launch.json)。
- 端口保护：1883 被占用时，Mosquitto 无法正常监听，需先释放端口或改监听端口。
- 进程保护：重复构建前请确保旧进程已退出；本工程构建脚本已内置结束旧进程与重试逻辑。
- 防火墙保护：若需局域网访问 Broker，需放通 TCP 1883。
- 版本保护：`build/`、`*.exe`、中间文件已纳入 `.gitignore`，避免提交二进制产物。

## 快速联调

1. 启动本地 MQTT Broker（Mosquitto）。
2. 启动本项目 GUI，点击连接。
3. 在 MQTTX 发布 JSON 到与 GUI 相同的主题。

示例主题：

- `mqttx_local_test`

示例消息：

```json
{
	"timestamp": "2026-04-09T12:40:00Z",
	"pitch": 1.25,
	"roll": -0.42,
	"x": 0.12,
	"y": -0.08,
	"z": 9.81
}
```

## Mosquitto 服务如何调整（Windows）

以下命令均在 PowerShell 执行。

### 1. 查看服务状态

```powershell
Get-Service mosquitto | Select-Object Name, Status, StartType
```

### 2. 启动/停止/重启服务

```powershell
Start-Service mosquitto
Stop-Service mosquitto
Restart-Service mosquitto
```

### 3. 设置开机自动启动或手动启动

```powershell
Set-Service mosquitto -StartupType Automatic
Set-Service mosquitto -StartupType Manual
```

### 4. 验证 1883 端口是否监听

```powershell
Get-NetTCPConnection -LocalPort 1883 -State Listen
```

若无输出，说明 Broker 未监听，需检查服务是否启动或配置是否正确。

### 5. 快速验证 Broker 可用性

```powershell
& "C:\Program Files\mosquitto\mosquitto_pub.exe" -h 127.0.0.1 -p 1883 -t test/ping -m "{\"ping\":1}"
& "C:\Program Files\mosquitto\mosquitto_sub.exe" -h 127.0.0.1 -p 1883 -t test/ping -v
```

先开 `mosquitto_sub`，再执行 `mosquitto_pub`，能收到消息即正常。

### 6. 调整监听地址与端口（最常用）

Mosquitto 主配置常见路径：

- `C:\Program Files\mosquitto\mosquitto.conf`

建议最小配置：

```conf
listener 1883 127.0.0.1
allow_anonymous true
```

修改后执行：

```powershell
Restart-Service mosquitto
```

如果希望局域网设备也能访问，可改为：

```conf
listener 1883 0.0.0.0
allow_anonymous true
```

并放通防火墙 TCP 1883。

### 7. 启用用户名密码鉴权（可选）

创建密码文件：

```powershell
& "C:\Program Files\mosquitto\mosquitto_passwd.exe" -c "C:\Program Files\mosquitto\passwd" molloyd
```

在 `mosquitto.conf` 加：

```conf
listener 1883 127.0.0.1
allow_anonymous false
password_file C:\Program Files\mosquitto\passwd
```

保存后重启服务：

```powershell
Restart-Service mosquitto
```

此时 Qt/MQTTX 需填写对应用户名和密码。

### 8. 常见问题排查

- 报 `ECONNREFUSED 127.0.0.1:1883`：服务没启动或没监听 1883。
- 能连接但收不到消息：发布主题与订阅主题不一致。
- 收到消息但无曲线：当前指标字段不在 JSON 里（例如选了 `cpu_temp`，消息里却只有 `pitch`）。
- 构建时报 `cannot open output file ... Permission denied`：程序正在运行，先关闭 GUI 再构建（本项目脚本已自动处理）。

## Qt 端参数建议（本项目）

- Broker 地址：`tcp://127.0.0.1:1883`
- 订阅主题：与 MQTTX 发布主题一致（例如 `mqttx_local_test`）
- 指标：选择与 JSON 字段匹配的项（`pitch`/`roll`/`x`/`y`/`z`/`cpu_temp`/`cpu_load`）

## 目录规范（已整理）

当前策略：

- 本文件夹以桌面端 Qt 工程为主（独立维护）。
- 树莓派端代码后续统一放在 `raspberry_pi/`，与 Qt 端解耦。

```text
.
├─src/                # Qt 源码（main/mainwindow/qcustomplot）
├─include/            # 头文件
├─ui/                 # Qt Designer .ui
├─tools/              # 构建与运行脚本
├─raspberry_pi/       # 树莓派端代码（预留）
├─sensor_node/ADXL345 # 传感器侧代码归档
├─.vscode/            # VS Code 任务与调试配置
├─build/              # 构建输出（已在 .gitignore 忽略）
├─EEN1071Ass2.pro
├─README.md
└─.gitignore
```

说明：

- Qt 工程已改为从 `src/include/ui` 引用文件。
- `tools/build_qt.ps1` 与 `tools/run_qt.ps1` 为统一入口。
- `build/` 和可执行文件已通过 `.gitignore` 排除，不会污染 Git 历史。

## Git 规范化流程

首次提交（在项目根目录执行）：

```powershell
git add .
git status
git commit -m "chore: normalize qt project structure and tooling"
```

日常开发建议：

```powershell
# 更新代码后构建
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tools\build_qt.ps1

# 启动程序
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tools\run_qt.ps1

# 提交改动
git add src include ui tools .vscode EEN1071Ass2.pro README.md .gitignore
git commit -m "feat: <你的变更说明>"
```

## 软件打包（给客户直接使用）

已提供一键打包脚本：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tools\package_qt.ps1
```

也可在 VS Code 任务中执行：

- `Qt: package portable zip`

打包输出：

- `dist/MQTTADXL345-portable/`：可直接运行目录
- `dist/MQTTADXL345-portable.zip`：可分发压缩包
- 主程序名称：`MQTTADXL345.exe`

分发建议：

1. 发送 `MQTTADXL345-portable.zip` 给客户。
2. 客户解压后双击 `run.bat`（或 `MQTTADXL345.exe`）。
3. 首次使用若提示 Broker 不可达，可按界面提示安装/启动 Mosquitto。

说明：

- 脚本会自动执行 `windeployqt` 收集 Qt 运行库。
- 脚本会尝试拷贝 Paho MQTT 运行时 DLL（若存在）。

