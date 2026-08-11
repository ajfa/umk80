@echo off
rem =====================================================================
rem  UMK-80 - paquete portable para Windows
rem
rem  Doble clic y listo. No hay nada que instalar: el panel usa Win32
rem  puro, que ya viene con el propio Windows.
rem
rem  Este fichero es ASCII puro a proposito. El cirilico y los acentos
rem  se los come la consola de Windows segun la pagina de codigos con la
rem  que arranque, asi que aqui no se usan: el panel de la ventana si los
rem  muestra bien.
rem =====================================================================

chcp 65001 >nul 2>&1
cd /d "%~dp0"

if not exist "umk80.exe" (
  echo No encuentro umk80.exe junto a este script.
  echo Este .cmd tiene que quedarse dentro de la carpeta del paquete.
  pause
  exit /b 1
)
if not exist "rom\monitor.bin" (
  echo No encuentro rom\monitor.bin. Falta la carpeta rom\ del paquete.
  pause
  exit /b 1
)

echo.
echo  ===================================================================
echo   UMK-80  --  banco didactico sovietico, VEF Riga, RR3.059.004
echo  ===================================================================
echo.
echo   COMO PROBARLO. Sigue estos seis pasos en la ventana que se abre.
echo   Puedes pulsar las teclas del teclado o hacer clic con el raton
echo   sobre las teclas dibujadas en el panel.
echo.
echo     1) Esc                     boton SB (reset).
echo                                Aparece un guion en el indicador
echo                                de mas a la izquierda.
echo.
echo     2) F1  y luego  0 8 0 0    directiva P (memoria) y la direccion.
echo        y luego espacio         El display muestra 0800 y el
echo                                contenido de esa celda.
echo.
echo     3) Teclea el programa, separando cada byte con espacio:
echo.
echo             3 E  espacio  A A  espacio  C 3  espacio
echo             0 0  espacio  0 8  espacio  Intro
echo.
echo        Acabas de meter  MVI A,0AAH / JMP 0800H  en la direccion 0800.
echo.
echo     4) F3  y luego  0 8 0 0    directiva ST (arrancar) y la direccion.
echo        y luego Intro           El programa se pone a correr en bucle.
echo.
echo     5) Retroceso               boton PR (interrupcion). Detiene el
echo                                programa y muestra donde se paro.
echo.
echo     6) F2  y luego  A          directiva RG (registros) y el registro A.
echo                                El display muestra:   A - AA
echo.
echo        Ese AA es el valor que el programa habia cargado en A. Si lo
echo        ves, el emulador esta funcionando de punta a punta.
echo.
echo  -------------------------------------------------------------------
echo   RESTO DE TECLAS
echo.
echo     0-9 A-F      teclas hexadecimales
echo     F1 F2 F3     P (memoria)   RG (registros)  ST (arrancar)
echo     F4 F5 F6     KS (suma)     ZK (rellenar)   PM (copiar)
echo     espacio      separador de parametros
echo     Intro        VP, fin de directiva
echo     Esc          SB, reset
echo     Retroceso    PR, interrupcion
echo     F8           SHG, avanzar un paso
echo     F9           RB/SHG, enclava el modo paso a paso
echo     F10          KM/CK, el paso pasa a ser por ciclo de maquina
echo.
echo   Cierra la ventana para salir.
echo  -------------------------------------------------------------------
echo.

umk80.exe --rom rom\monitor.bin

if errorlevel 1 (
  echo.
  echo El emulador termino con error. Deja esta ventana abierta y
  echo mira el mensaje de arriba.
  pause
)
