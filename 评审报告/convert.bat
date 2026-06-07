@echo off
chcp 65001 >/dev/null
if "%~1"=="" (
  echo 用法: 把 .autosp 文件拖到本 bat 上, 或  convert.bat 报告.autosp [输出.csv]
  pause & exit /b
)
"%~dp0Autosp2Csv.exe" %*
echo.
pause
