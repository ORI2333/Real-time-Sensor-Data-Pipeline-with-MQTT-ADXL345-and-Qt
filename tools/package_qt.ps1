$ErrorActionPreference = "Stop"

$workspace = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $workspace "build"
$releaseDir = Join-Path $buildDir "release"
$sourceExeName = "EEN1071Ass2.exe"
$packageExeName = "MQTTADXL345.exe"
$exePath = Join-Path $releaseDir $sourceExeName

$qmake = "E:/Qt/5.14.2/mingw73_64/bin/qmake.exe"
$makeExe = "E:/Qt/Tools/mingw730_64/bin/mingw32-make.exe"
$windeployqt = "E:/Qt/5.14.2/mingw73_64/bin/windeployqt.exe"
$qtBin = "E:/Qt/5.14.2/mingw73_64/bin"
$qtPlugins = "E:/Qt/5.14.2/mingw73_64/plugins"
$buildScript = Join-Path $workspace "tools/build_qt.ps1"

$distRoot = Join-Path $workspace "dist"
$packageName = "MQTTADXL345-portable"
$packageDir = Join-Path $distRoot $packageName
$zipPath = Join-Path $distRoot "$packageName.zip"

$pahoBin = "E:/E_EngineeringWarehouse/202604/RTSP_MQTT_QtOfficial_OneShot/third_party/paho/install/bin"
$pahoLib = "E:/E_EngineeringWarehouse/202604/RTSP_MQTT_QtOfficial_OneShot/third_party/paho/install/lib"

if (!(Test-Path $exePath)) {
    Write-Host "Release exe not found, building first..."
    & pwsh -NoProfile -ExecutionPolicy Bypass -File $buildScript
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed, package aborted."
    }
}

if (!(Test-Path $windeployqt)) {
    throw "windeployqt not found: $windeployqt"
}

function Copy-QtRuntimeFallback {
    param(
        [Parameter(Mandatory = $true)]
        [string]$TargetDir
    )

    $qtDlls = @(
        "Qt5Core.dll",
        "Qt5Gui.dll",
        "Qt5Widgets.dll",
        "Qt5Network.dll",
        "Qt5PrintSupport.dll",
        "libgcc_s_seh-1.dll",
        "libstdc++-6.dll",
        "libwinpthread-1.dll"
    )

    foreach ($dll in $qtDlls) {
        $src = Join-Path $qtBin $dll
        if (Test-Path $src) {
            Copy-Item $src (Join-Path $TargetDir $dll) -Force
        }
    }

    $platformDir = Join-Path $TargetDir "platforms"
    if (!(Test-Path $platformDir)) {
        New-Item -ItemType Directory -Path $platformDir | Out-Null
    }
    $qwindows = Join-Path $qtPlugins "platforms/qwindows.dll"
    if (Test-Path $qwindows) {
        Copy-Item $qwindows (Join-Path $platformDir "qwindows.dll") -Force
    }

    $stylesDir = Join-Path $TargetDir "styles"
    if (!(Test-Path $stylesDir)) {
        New-Item -ItemType Directory -Path $stylesDir | Out-Null
    }
    $styleDll = Join-Path $qtPlugins "styles/qwindowsvistastyle.dll"
    if (Test-Path $styleDll) {
        Copy-Item $styleDll (Join-Path $stylesDir "qwindowsvistastyle.dll") -Force
    }

    $imgDir = Join-Path $TargetDir "imageformats"
    if (!(Test-Path $imgDir)) {
        New-Item -ItemType Directory -Path $imgDir | Out-Null
    }
    foreach ($img in @("qico.dll", "qjpeg.dll", "qgif.dll")) {
        $srcImg = Join-Path $qtPlugins ("imageformats/" + $img)
        if (Test-Path $srcImg) {
            Copy-Item $srcImg (Join-Path $imgDir $img) -Force
        }
    }
}

if (!(Test-Path $distRoot)) {
    New-Item -ItemType Directory -Path $distRoot | Out-Null
}

if (Test-Path $packageDir) {
    Remove-Item -Recurse -Force $packageDir
}
if (Test-Path $zipPath) {
    Remove-Item -Force $zipPath
}

New-Item -ItemType Directory -Path $packageDir | Out-Null

Copy-Item $exePath (Join-Path $packageDir $packageExeName) -Force

# Collect Qt runtime dependencies for a portable package.
& $windeployqt --release --no-translations --compiler-runtime (Join-Path $packageDir $packageExeName)
if ($LASTEXITCODE -ne 0) {
    Write-Warning "windeployqt failed, using fallback runtime copy."
    Copy-QtRuntimeFallback -TargetDir $packageDir
}

# Copy MQTT runtime DLLs if present.
$copiedPaho = @()
if (Test-Path $pahoBin) {
    Get-ChildItem $pahoBin -Filter "*.dll" -ErrorAction SilentlyContinue | ForEach-Object {
        Copy-Item $_.FullName (Join-Path $packageDir $_.Name) -Force
        $copiedPaho += $_.Name
    }
}
if (Test-Path $pahoLib) {
    Get-ChildItem $pahoLib -Filter "*.dll" -ErrorAction SilentlyContinue | ForEach-Object {
        if (!(Test-Path (Join-Path $packageDir $_.Name))) {
            Copy-Item $_.FullName (Join-Path $packageDir $_.Name) -Force
            $copiedPaho += $_.Name
        }
    }
}

$iconPath = Join-Path $workspace "assets/app_icon.ico"
if (Test-Path $iconPath) {
    Copy-Item $iconPath (Join-Path $packageDir "app_icon.ico") -Force
}

Copy-Item (Join-Path $workspace "README.md") (Join-Path $packageDir "README.md") -Force

$runBat = @"
@echo off
cd /d %~dp0
start "" MQTTADXL345.exe
"@
Set-Content -Path (Join-Path $packageDir "run.bat") -Value $runBat -Encoding ASCII

$portableReadme = @"
MQTTADXL345 Portable Package

1) Double-click run.bat (or MQTTADXL345.exe).
2) Ensure MQTT broker is reachable (default: 127.0.0.1:1883).
3) If local broker is not installed, install/start Mosquitto first.

Bundled MQTT runtime DLLs:
$($copiedPaho -join ', ')
"@
Set-Content -Path (Join-Path $packageDir "README_PORTABLE.txt") -Value $portableReadme -Encoding UTF8

Compress-Archive -Path (Join-Path $packageDir "*") -DestinationPath $zipPath -CompressionLevel Optimal

Write-Host "Package folder: $packageDir"
Write-Host "Package zip:    $zipPath"
