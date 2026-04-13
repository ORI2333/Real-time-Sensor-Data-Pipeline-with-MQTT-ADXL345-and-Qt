$ErrorActionPreference = "Stop"

$workspace = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $workspace "build"
$exePath = Join-Path $buildDir "release\EEN1071Ass2.exe"

if (!(Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

Set-Location $buildDir

# A running GUI process will lock the output exe and make linking fail.
$running = Get-Process -Name "EEN1071Ass2" -ErrorAction SilentlyContinue
if ($running) {
    Write-Host "Stopping running EEN1071Ass2 instances before build..."
    $running | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 400
}

Remove-Item -Force -ErrorAction SilentlyContinue Makefile,Makefile.Release,Makefile.Debug,.qmake.stash

$qmake = "E:/Qt/5.14.2/mingw73_64/bin/qmake.exe"
$makeExe = "E:/Qt/Tools/mingw730_64/bin/mingw32-make.exe"
$projectFile = "../EEN1071Ass2.pro"
$pahoInclude = "INCLUDEPATH+=E:/E_EngineeringWarehouse/202604/RTSP_MQTT_QtOfficial_OneShot/third_party/paho/install/include"
$pahoLib = "LIBS+=-LE:/E_EngineeringWarehouse/202604/RTSP_MQTT_QtOfficial_OneShot/third_party/paho/install/lib"

& $qmake $projectFile -spec win32-g++ "QMAKE_MAKE=$makeExe" $pahoInclude $pahoLib
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $makeExe -j4
if ($LASTEXITCODE -ne 0) {
    # Retry once for transient file lock situations.
    $running = Get-Process -Name "EEN1071Ass2" -ErrorAction SilentlyContinue
    if ($running) {
        Write-Host "Build failed; retrying after stopping EEN1071Ass2..."
        $running | Stop-Process -Force -ErrorAction SilentlyContinue
        Start-Sleep -Milliseconds 500
        & $makeExe -j4
    }
}

exit $LASTEXITCODE
