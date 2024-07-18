@echo off
SET ARCH=arm64
SET XVC_ARCH=x86
IF "%1" EQU "debug" (SET MODE=debug) ELSE SET MODE=release
CALL xx_release\build_windows.bat