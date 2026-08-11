@echo off
rem =====================================================================
rem  УМК-80 — paquete portable para Windows
rem
rem  Doble clic y listo. No hay nada que instalar: el panel usa Win32
rem  puro, que va en el propio Windows.
rem =====================================================================

cd /d "%~dp0"

if not exist "umk80.exe" (
  echo No encuentro umk80.exe junto a este script.
  echo Este .cmd tiene que quedarse dentro de la carpeta del paquete.
  pause
  exit /b 1
)

if not exist "rom\monitor.bin" (
  echo No encuentro rom\monitor.bin.
  pause
  exit /b 1
)

echo.
echo   УМК-80 - Учебный микропроцессорный комплект  (ВЭФ, РР3.059.004)
echo.
echo   Teclado:  0-9 A-F        teclas hexadecimales
echo             F1..F6         P  RG  ST  KS  ZK  PM
echo             espacio        separador de parametros
echo             Intro          VP  (fin de directiva)
echo             Esc            SB  (reset)
echo             Retroceso      PR  (interrupcion)
echo             F8 / F9 / F10  SHG / RB-SHG / KM-CK
echo.
echo   Prueba rapida, tecleando en el panel:
echo             SB, P, 0800, espacio, 3E, espacio, AA, espacio,
echo             C3, espacio, 00, espacio, 08, espacio, VP
echo             luego ST, 0800, VP     y despues PR, RG, A
echo.

umk80.exe --rom rom\monitor.bin
