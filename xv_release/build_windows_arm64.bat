@echo off
ertbuild xv_com\xv.ertproj -Nar arm64
ertbuild xv_sl\xvsl.ertproj -Nar arm64
ertbuild xv_mm\xvm.ertproj -Nar arm64
ertbuild xi_tool\xi.ertproj -Nar arm64
mkdir xv_release\_build
del /S /Q xv_release\_build\windows_arm64\*
mkdir xv_release\_build\windows_arm64
copy /B xv_mm\_build\windows_arm64_release\xvm.exe xv_release\_build\windows_arm64\xvm.exe
mkdir xv_release\_build\windows_arm64\xvcl
mkdir xv_release\_build\windows_arm64\vscx
mkdir xv_release\_build\windows_arm64\xv.loc
mkdir xv_release\_build\windows_arm64\xi.loc
copy /B xv_com\xv.ini xv_release\_build\windows_arm64\xv.ini
copy /B xv_com\xe.ini xv_release\_build\windows_arm64\xe.ini
copy /B xi_tool\xi.ini xv_release\_build\windows_arm64\xi.ini
copy /B xv_com\_build\windows_arm64_release\xv.exe xv_release\_build\windows_arm64\xv.exe
copy /B xv_sl\_build\windows_arm64_release\xvsl.exe xv_release\_build\windows_arm64\xvsl.exe
copy /B xi_tool\_build\windows_arm64_release\xi.exe xv_release\_build\windows_arm64\xi.exe
copy /B xv_release\engine-xv-vscx-1.0.0.vsix xv_release\_build\windows_arm64\vscx\xv-vscx.vsix
copy /B xv_lib\manualis.xvm xv_release\_build\windows_arm64\xvcl\
copy /B xv_lib\canonicalis.xvm xv_release\_build\windows_arm64\xvcl\
copy /B xv_lib\limae.xvm xv_release\_build\windows_arm64\xvcl\
copy /B xv_lib\imago.xvm xv_release\_build\windows_arm64\xvcl\
copy /B xv_lib\consolatorium.xvm xv_release\_build\windows_arm64\xvcl\
.\xv_release\_build\windows_x64\xv.exe xv_lib\canonicalis.xv -NO xv_release\_build\windows_arm64\xvcl
.\xv_release\_build\windows_x64\xv.exe xv_lib\limae.xv -NO xv_release\_build\windows_arm64\xvcl
.\xv_release\_build\windows_x64\xv.exe xv_lib\imago.xv -NO xv_release\_build\windows_arm64\xvcl
.\xv_release\_build\windows_x64\xv.exe xv_lib\consolatorium.xv -NO xv_release\_build\windows_arm64\xvcl
estrtab xv_com\locale\ru.txt :binary xv_release\_build\windows_arm64\xv.loc\ru.ecst
estrtab xv_com\locale\en.txt :binary xv_release\_build\windows_arm64\xv.loc\en.ecst
estrtab xi_tool\locale\ru.txt :binary xv_release\_build\windows_arm64\xi.loc\ru.ecst
estrtab xi_tool\locale\en.txt :binary xv_release\_build\windows_arm64\xi.loc\en.ecst