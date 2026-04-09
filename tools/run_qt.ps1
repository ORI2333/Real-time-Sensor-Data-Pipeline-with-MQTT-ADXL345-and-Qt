$ErrorActionPreference = "Stop"

$workspace = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $workspace "build\release\EEN1071Ass2.exe"

if (!(Test-Path $exe)) {
    Write-Error "Executable not found: $exe"
    exit 2
}

$env:PATH = "E:/Qt/5.14.2/mingw73_64/bin;E:/Qt/Tools/mingw730_64/bin;E:/E_EngineeringWarehouse/202604/RTSP_MQTT_QtOfficial_OneShot/third_party/paho/install/bin;" + $env:PATH

$p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe -Parent) -PassThru

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win32 {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool ShowWindowAsync(IntPtr hWnd, int nCmdShow);
}
"@

for ($i = 0; $i -lt 20; $i++) {
    Start-Sleep -Milliseconds 150
    $p.Refresh()
    if ($p.HasExited) {
        break
    }
    if ($p.MainWindowHandle -ne 0) {
        [Win32]::ShowWindowAsync($p.MainWindowHandle, 9) | Out-Null
        [Win32]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
        break
    }
}

Write-Host ("Started EEN1071Ass2, PID=" + $p.Id)
