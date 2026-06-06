@echo off
setlocal

cd /d "%~dp0"

if not defined FROMELF (
    for /f "delims=" %%I in ('where fromelf 2^>nul') do (
        set "FROMELF=%%I"
        goto fromelf_found
    )
)

if not defined FROMELF if exist "%LOCALAPPDATA%\Keil_v5\ARM\ARMCOMPILER506\bin\fromelf.exe" (
    set "FROMELF=%LOCALAPPDATA%\Keil_v5\ARM\ARMCOMPILER506\bin\fromelf.exe"
)

if not defined FROMELF if exist "%LOCALAPPDATA%\Keil_v5\ARM\ARMCLANG\bin\fromelf.exe" (
    set "FROMELF=%LOCALAPPDATA%\Keil_v5\ARM\ARMCLANG\bin\fromelf.exe"
)

:fromelf_found
if not defined FROMELF (
    echo fromelf.exe not found. Add it to PATH or set FROMELF to its full path.
    exit /b 1
)

if exist "Project.axf" goto make_in_output
if exist ".\output\Project.axf" goto make_in_project

echo Project.axf not found.
exit /b 1

:make_in_output
"%FROMELF%" --bin --output="App.bin" "Project.axf"
if errorlevel 1 exit /b %ERRORLEVEL%
set "APP_BIN=App.bin"
goto done

:make_in_project
"%FROMELF%" --bin --output=".\output\App.bin" ".\output\Project.axf"
if errorlevel 1 exit /b %ERRORLEVEL%
set "APP_BIN=.\output\App.bin"
goto done

:done
echo APP bin: %APP_BIN%
exit /b 0
