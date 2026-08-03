@echo off
setlocal ENABLEDELAYEDEXPANSION

rem Qt6 base path (modify if installed elsewhere)
set QT_BASE=D:\Qt\6.8.1\mingw_64
set MINGW_TOOLS=D:\Qt\Tools\mingw1310_64\bin

set APP_DIR=%~dp0build\release
set APP_EXE=%APP_DIR%\ctdy123.exe

rem Prepare runtime environment for Qt6 MinGW
set PATH=%QT_BASE%\bin;%MINGW_TOOLS%;%APP_DIR%;%PATH%


echo Starting application...
start "ctdy123" "%APP_EXE%"
exit /b 0


