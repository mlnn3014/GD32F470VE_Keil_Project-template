@echo off
setlocal
cd /d "%~dp0"

set "PACKER=.\Tools\ota_stream_pack.exe"
set "CONFIG=..\HeaderFiles\common\ota_package_config.h"
set "AXF=.\output\Project.axf"
set "APP_BIN=.\output\App.bin"
set "OTA_BIN=.\output\App_ota.bin"
set "FROMELF=fromelf"

if not exist "%PACKER%" (
    echo [OTA] packer not found: %PACKER%
    goto fail
)

where "%FROMELF%" >nul 2>nul
if errorlevel 1 (
    if exist "D:\Keil\ARM\ARMCC\Bin\fromelf.exe" (
        set "FROMELF=D:\Keil\ARM\ARMCC\Bin\fromelf.exe"
    )
)

if not exist "%AXF%" (
    echo [OTA] AXF not found: %AXF%
    echo [OTA] Skip AXF export and use existing App.bin.
) else if not "%FROMELF%"=="fromelf" (
    echo [OTA] Exporting raw App.bin...
    "%FROMELF%" --bin --output="%APP_BIN%" "%AXF%"
    if errorlevel 1 goto fail
) else (
    where fromelf >nul 2>nul
    if errorlevel 1 (
        echo [OTA] fromelf not found in PATH.
        echo [OTA] Skip AXF export and use existing App.bin.
    ) else (
        echo [OTA] Exporting raw App.bin...
        fromelf --bin --output="%APP_BIN%" "%AXF%"
        if errorlevel 1 goto fail
    )
)

if not exist "%APP_BIN%" (
    echo [OTA] App.bin not found: %APP_BIN%
    echo [OTA] Build the Keil project first, or export App.bin manually.
    goto fail
)

echo [OTA] Packing OTA image...
"%PACKER%" --config "%CONFIG%" --bin "%APP_BIN%" --out "%OTA_BIN%"
if errorlevel 1 goto fail

echo [OTA] Done: %OTA_BIN%
echo [OTA] Send this file with serial tool.
pause
exit /b 0

:fail
echo [OTA] Failed.
pause
exit /b 1

