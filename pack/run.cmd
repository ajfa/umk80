@echo off
rem =====================================================================
rem  UMK-80 - portable package for Windows
rem
rem  Double-click and go. Nothing to install: the panel uses plain Win32,
rem  which ships with Windows itself.
rem
rem  This file is deliberately pure ASCII. A Windows console mangles
rem  Cyrillic and accents depending on the code page it starts in, so none
rem  are used here; the panel window does show them correctly.
rem =====================================================================

chcp 65001 >nul 2>&1
cd /d "%~dp0"

if not exist "umk80.exe" (
  echo Cannot find umk80.exe next to this script.
  echo This .cmd has to stay inside the package folder.
  pause
  exit /b 1
)
if not exist "rom\monitor.bin" (
  echo Cannot find rom\monitor.bin. The package's rom\ folder is missing.
  pause
  exit /b 1
)

echo.
echo  ===================================================================
echo   UMK-80  --  Soviet educational trainer, VEF Riga, RR3.059.004
echo  ===================================================================
echo.
echo   HOW TO TRY IT. Follow these six steps in the window that opens.
echo   You can press the keys on your keyboard or click the drawn keys
echo   on the panel with the mouse.
echo.
echo     1) Esc                     the SB button (reset).
echo                                A dash appears on the leftmost
echo                                display.
echo.
echo     2) F1  then  0 8 0 0       the P directive (memory) and the
echo        then space              address. The display shows 0800 and
echo                                the contents of that cell.
echo.
echo     3) Type the program, separating each byte with space:
echo.
echo             3 E  space  A A  space  C 3  space
echo             0 0  space  0 8  space  Enter
echo.
echo        You have just entered  MVI A,0AAH / JMP 0800H  at 0800.
echo.
echo     4) F3  then  0 8 0 0       the ST directive (start) and the
echo        then Enter              address. The program starts looping.
echo.
echo     5) Backspace               the PR button (interrupt). It stops
echo                                the program and shows where.
echo.
echo     6) F2  then  A             the RG directive (registers) and
echo                                register A. The display shows:
echo.
echo                                    A - AA
echo.
echo        That AA is the value the program had loaded into A. If you
echo        see it, the emulator is working end to end.
echo.
echo  -------------------------------------------------------------------
echo   ALL THE KEYS
echo.
echo     0-9 A-F      hexadecimal keys
echo     F1 F2 F3     P (memory)    RG (registers)  ST (start)
echo     F4 F5 F6     KS (checksum) ZK (fill)       PM (copy)
echo     space        parameter separator
echo     Enter        VP, end of directive
echo     Esc          SB, reset
echo     Backspace    PR, interrupt
echo     F8           SHG, advance one step
echo     F9           RB/SHG, latches single-step mode
echo     F10          KM/CK, makes each step one machine cycle
echo.
echo   Close the window to quit.
echo  -------------------------------------------------------------------
echo.

umk80.exe --rom rom\monitor.bin

if errorlevel 1 (
  echo.
  echo The emulator exited with an error. Leave this window open and
  echo read the message above.
  pause
)
