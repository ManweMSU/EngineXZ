@echo off
CALL xv_release\build_windows_x86.bat
CALL xv_release\build_windows_x64.bat
CALL xv_release\build_windows_arm64.bat
CALL xx_release\build_windows_x86.bat
CALL xx_release\build_windows_x64.bat
CALL xx_release\build_windows_arm64.bat
xv com_release/arc.xv -Ndr "xv_release/_build/windows_x86"^
    com_release/manifesta/xv_win.manifest com_release/xv_10_b6_win_x86.ecsa
xv com_release/arc.xv -Ndr "xv_release/_build/windows_x64"^
    com_release/manifesta/xv_win.manifest com_release/xv_10_b6_win_x64.ecsa
xv com_release/arc.xv -Ndr "xv_release/_build/windows_arm64"^
    com_release/manifesta/xv_win.manifest com_release/xv_10_b6_win_arm64.ecsa
xv com_release/arc.xv -Ndr "xx_release/_build/windows_x86"^
    com_release/manifesta/xx_win.manifest com_release/xx_10_b6_win_x86.ecsa
xv com_release/arc.xv -Ndr "xx_release/_build/windows_x64"^
    com_release/manifesta/xx_win.manifest com_release/xx_10_b6_win_x64.ecsa
xv com_release/arc.xv -Ndr "xx_release/_build/windows_arm64"^
    com_release/manifesta/xx_win.manifest com_release/xx_10_b6_win_arm64.ecsa
CALL purify.bat