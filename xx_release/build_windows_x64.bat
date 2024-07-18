@echo off
SET ARCH=x64
SET XVC_ARCH=x64
IF "%1" EQU "debug" (SET MODE=debug) ELSE SET MODE=release
CALL xx_release\build_windows.bat