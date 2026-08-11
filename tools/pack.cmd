@echo off
rem =====================================================================
rem  Builds the portable Windows package: build\umk80-windows.zip
rem
rem  It first checks that the executables depend on no DLL beyond the ones
rem  that ship with Windows itself.
rem =====================================================================
setlocal
cd /d "%~dp0\.."

set STAGE=build\pack\umk80-windows

if not exist "build\umk80.exe" (
  echo Build first:  mingw32-make
  exit /b 1
)

if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%\rom"  2>nul
mkdir "%STAGE%\docs" 2>nul

copy /y "build\umk80.exe"  "%STAGE%\" >nul
copy /y "build\umkcli.exe" "%STAGE%\" >nul
copy /y "build\umkasm.exe" "%STAGE%\" >nul
copy /y "build\umkdis.exe" "%STAGE%\" >nul
copy /y "build\umkrom.exe" "%STAGE%\" >nul

copy /y "rom\monitor.bin"  "%STAGE%\rom\" >nul
copy /y "rom\monitor.lst"  "%STAGE%\rom\" >nul
copy /y "rom\monitor.asm"  "%STAGE%\rom\" >nul

copy /y "pack\run.cmd"       "%STAGE%\" >nul
copy /y "README.md"          "%STAGE%\" >nul
copy /y "PLAN.md"            "%STAGE%\" >nul
copy /y "DESCONOCIDOS.md"    "%STAGE%\" >nul
copy /y "docs\FUENTES.md"    "%STAGE%\docs\" >nul

echo Checking DLL dependencies...
for %%E in (umk80 umkcli umkasm umkdis umkrom) do (
  objdump -p "%STAGE%\%%E.exe" | findstr /c:"DLL Name"
)

if exist "build\umk80-windows.zip" del "build\umk80-windows.zip"
powershell -NoProfile -Command ^
  "Compress-Archive -Path 'build/pack/umk80-windows/*' -DestinationPath 'build/umk80-windows.zip' -Force"

echo.
echo Done: build\umk80-windows.zip
endlocal
