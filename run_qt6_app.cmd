@echo off
setlocal ENABLEDELAYEDEXPANSION

rem Qt6 base path (modify if installed elsewhere)
set QT_BASE=D:\Qt\6.8.1\mingw_64
set MINGW_TOOLS=D:\Qt\Tools\mingw1310_64\bin

set APP_DIR=%~dp0build\release
set APP_EXE=%APP_DIR%\ctdy123.exe

if not exist "%APP_EXE" (
  echo [INFO] App not built yet. Building...
  call "%~dp0build_qt6_release.cmd" || exit /b 1
)

rem Prepare runtime environment for Qt6 MinGW
set PATH=%QT_BASE%\bin;%MINGW_TOOLS%;%APP_DIR%;%PATH%

rem Deploy Qt runtime DLLs alongside the exe
where windeployqt.exe >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
  echo [WARN] windeployqt not found in PATH; trying Qt Creator Tools fallback.
  set WDEP=D:\Qt\Tools\QtCreator\bin\windeployqt.exe
else (
  for /f "delims=" %%i in ('where windeployqt.exe') do set WDEP=%%i
)

if exist "%WDEP%" (
  echo Running windeployqt...
  "%WDEP%" --compiler-runtime --no-translations --no-opengl-sw "%APP_EXE%"
) else (
  echo [WARN] windeployqt not available; attempting to run with PATH-based DLL resolution.
)

echo Starting application...
start "ctdy123" "%APP_EXE%"
exit /b 0


