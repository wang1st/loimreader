@echo off
setlocal ENABLEDELAYEDEXPANSION

rem Build (Release) then deploy Qt runtime and package into dist folder

set "QT_BASE=D:\Qt\6.8.1\mingw_64"
set "QT_TOOLS=D:\Qt\6.8.1\mingw_64\bin"
set "MINGW_TOOLS=D:\Qt\Tools\mingw1310_64\bin"

set "WDEP=%QT_TOOLS%\windeployqt.exe"
if not exist "%WDEP%" (
  echo [WARN] windeployqt not found at %WDEP%
  echo        Please adjust QT_TOOLS to your Qt Creator path.
)

echo Building release...
if not exist build (
  mkdir build
)
cd build
"%QT_BASE%\bin\qmake.exe" .. || goto :error
"%MINGW_TOOLS%\mingw32-make.exe" -j4 || goto :error

set "OUT=release\ctdy123.exe"
if not exist "%OUT%" (
  echo [ERROR] Build output not found: %CD%\%OUT%
  goto :error
)

echo Preparing dist directory...
cd ..
if exist dist rmdir /S /Q dist
mkdir dist
mkdir dist\bin
copy /Y build\release\ctdy123.exe dist\bin\ >nul
copy /Y icons\main.ico dist\bin\ >nul

if exist "%WDEP%" (
  echo Running windeployqt...
  "%WDEP%" --compiler-runtime --no-translations --no-opengl-sw dist\bin\ctdy123.exe
) else (
  echo [WARN] windeployqt not available; packaging without auto-Qt deploy.
)

echo Copying resources...
copy /Y clps.qrc dist\ >nul
if not exist dist\i18n mkdir dist\i18n
copy /Y i18n\*.qm dist\i18n\ >nul
if not exist dist\icons mkdir dist\icons
copy /Y icons\*.png dist\icons\ >nul

echo Generating portable launcher (run.cmd)...
>dist\run.cmd echo @echo off
>>dist\run.cmd echo cd /d %%~dp0\bin
>>dist\run.cmd echo start "ctdy123" ctdy123.exe

echo Done. Dist at %CD%\dist
echo Next: build installer with installer.iss using Inno Setup.
exit /b 0

:error
echo [ERROR] Packaging failed.
exit /b 1


