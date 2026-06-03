@echo off
setlocal

if not defined FROMELF (
    for /f "delims=" %%I in ('where fromelf 2^>nul') do (
        set "FROMELF=%%I"
        goto fromelf_found
    )
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
exit /b %ERRORLEVEL%

:make_in_project
"%FROMELF%" --bin --output=".\output\App.bin" ".\output\Project.axf"
exit /b %ERRORLEVEL%
