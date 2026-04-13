# MQTT ADXL345 Real-Time Monitoring Panel (Qt)

This project is a local desktop client built with Qt Widgets and MQTT (Paho C). It receives JSON sensor payloads, renders real-time charts, and calculates rolling 60-second statistics.

## Read First (Submission / Report)

- Full technical documentation: `docs/系统说明文档.md`
- Raspberry Pi quick notes: `raspberry_pi/README.md`

## Dependencies

### OS

- Windows 10/11 (x64)

### Qt Toolchain

- Qt 5.14.2 (MinGW 7.3 64-bit)
- qmake
- mingw32-make
- gdb (for VS Code debugging)

### MQTT

- Eclipse Paho MQTT C
- Mosquitto broker
- MQTTX (optional for manual testing)

### Recommended Tools

- VS Code
- C/C++ extension (`ms-vscode.cpptools`)
- PowerShell 7 or Windows PowerShell

## Quick Environment Check

```powershell
& "E:\Qt\5.14.2\mingw73_64\bin\qmake.exe" -v
& "E:\Qt\Tools\mingw730_64\bin\mingw32-make.exe" -v
Get-NetTCPConnection -LocalPort 1883 -State Listen
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tools\build_qt.ps1
```

## Notes

- Keep source files in UTF-8.
- If Qt paths differ, update script and VS Code config files accordingly.
- If port 1883 is occupied, Mosquitto cannot bind normally.
- Stop old processes before rebuilding.

## Quick Integration Test

1. Start Mosquitto.
2. Start this GUI and connect.
3. Publish JSON from MQTTX to the subscribed topic.

Example topic: `mqttx_local_test`

Example payload:

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

## Qt Parameter Suggestion

- Broker: `tcp://127.0.0.1:1883`
- Topic: same as your publisher/test tool
- Metric: choose fields available in JSON (`pitch` / `roll` / `x` / `y` / `z` / `cpu_temp` / `cpu_load`)
