@echo off
CALL xv_release\build_windows_x86.bat
CALL xv_release\build_windows_x64.bat
CALL xv_release\build_windows_arm64.bat
CALL xx_release\build_windows_x86.bat
CALL xx_release\build_windows_x64.bat
CALL xx_release\build_windows_arm64.bat
ecfxpack com_release\xv-windows.ecsa -d xv_release\_build\windows_x86 windows-x86 ""^
                                     -d xv_release\_build\windows_x64 windows-x64 ""^
                                     -d xv_release\_build\windows_arm64 windows-arm64 ""
ecfxpack com_release\xx-windows.ecsa -d xx_release\_build\windows_x86 windows-x86 ""^
                                     -d xx_release\_build\windows_x64 windows-x64 ""^
                                     -d xx_release\_build\windows_arm64 windows-arm64 ""
CALL purify.bat