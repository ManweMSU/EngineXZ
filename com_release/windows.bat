@echo off
CALL xv_release\build_windows_x86.bat
CALL xv_release\build_windows_x64.bat
CALL xv_release\build_windows_arm64.bat
CALL xx_release\build_windows_x86.bat
CALL xx_release\build_windows_x64.bat
CALL xx_release\build_windows_arm64.bat
xv com_release/fid.xv -Ndr "xv_release/_build/windows_x86/xi"^
    "xx_release/_build/windows_x86/xxcl"^
    "xx_release/_build/windows_x86/xxi"^
    "xx_release/_build/windows_x64/xxcl"^
    "xx_release/_build/windows_x64/xxi"^
    "xx_release/_build/windows_arm64/xxcl"^
    "xx_release/_build/windows_arm64/xxi"
xv com_release/arc.xv -Ndr "xv_release/_build/windows_x86"^
    com_release/manifesta/xv_win.manifest com_release/xv_10_b7_win_x86.ecsa
xv com_release/arc.xv -Ndr "xv_release/_build/windows_x64"^
    com_release/manifesta/xv_win.manifest com_release/xv_10_b7_win_x64.ecsa
xv com_release/arc.xv -Ndr "xv_release/_build/windows_arm64"^
    com_release/manifesta/xv_win.manifest com_release/xv_10_b7_win_arm64.ecsa
xv com_release/arc.xv -Ndr "xx_release/_build/windows_x86"^
    com_release/manifesta/xx_win.manifest com_release/xx_10_b7_win_x86.ecsa
xv com_release/arc.xv -Ndr "xx_release/_build/windows_x64"^
    com_release/manifesta/xx_win.manifest com_release/xx_10_b7_win_x64.ecsa
xv com_release/arc.xv -Ndr "xx_release/_build/windows_arm64"^
    com_release/manifesta/xx_win.manifest com_release/xx_10_b7_win_arm64.ecsa
CALL purify.bat