@echo off
ertbuild xv_com\xv.ertproj -Nar x64
ertbuild xv_sl\xvsl.ertproj -Nar x64
ertbuild xv_mm\xvm.ertproj -Nar x64
ertbuild xi_tool\xi.ertproj -Nar x64
mkdir xv_release\_build
del /S /Q xv_release\_build\windows_x64\*
mkdir xv_release\_build\windows_x64
copy /B xv_mm\_build\windows_x64_release\xvm.exe xv_release\_build\windows_x64\xvm.exe
mkdir xv_release\_build\windows_x64\xvcl
mkdir xv_release\_build\windows_x64\vscx
mkdir xv_release\_build\windows_x64\xv.loc
mkdir xv_release\_build\windows_x64\xi.loc
copy /B xv_com\xv.ini xv_release\_build\windows_x64\xv.ini
copy /B xv_com\xe.ini xv_release\_build\windows_x64\xe.ini
copy /B xi_tool\xi.ini xv_release\_build\windows_x64\xi.ini
copy /B xv_com\_build\windows_x64_release\xv.exe xv_release\_build\windows_x64\xv.exe
copy /B xv_sl\_build\windows_x64_release\xvsl.exe xv_release\_build\windows_x64\xvsl.exe
copy /B xi_tool\_build\windows_x64_release\xi.exe xv_release\_build\windows_x64\xi.exe
copy /B xv_release\engine-xv-vscx-1.0.0.vsix xv_release\_build\windows_x64\vscx\xv-vscx.vsix
copy /B xv_lib\manualis.xvm xv_release\_build\windows_x64\xvcl\
copy /B xv_lib\canonicalis.xvm xv_release\_build\windows_x64\xvcl\
copy /B xv_lib\limae.xvm xv_release\_build\windows_x64\xvcl\
copy /B xv_lib\imago.xvm xv_release\_build\windows_x64\xvcl\
copy /B xv_lib\consolatorium.xvm xv_release\_build\windows_x64\xvcl\
copy /B xv_lib\formati.xvm xv_release\_build\windows_x64\xvcl\
copy /B xv_lib\lxx.xvm xv_release\_build\windows_x64\xvcl\
copy /B xv_lib\potentia.xvm xv_release\_build\windows_x64\xvcl\
.\xv_release\_build\windows_x64\xv.exe xv_lib\canonicalis.xv -NO xv_release\_build\windows_x64\xvcl
.\xv_release\_build\windows_x64\xv.exe xv_lib\limae.xv -NO xv_release\_build\windows_x64\xvcl
.\xv_release\_build\windows_x64\xv.exe xv_lib\imago.xv -NO xv_release\_build\windows_x64\xvcl
.\xv_release\_build\windows_x64\xv.exe xv_lib\consolatorium.xv -NO xv_release\_build\windows_x64\xvcl
.\xv_release\_build\windows_x64\xv.exe xv_lib\formati.xv -NO xv_release\_build\windows_x64\xvcl
.\xv_release\_build\windows_x64\xv.exe xv_lib\lxx.xv -NO xv_release\_build\windows_x64\xvcl
.\xv_release\_build\windows_x64\xv.exe xv_lib\potentia.xv -NO xv_release\_build\windows_x64\xvcl
estrtab xv_com\locale\ru.txt :binary xv_release\_build\windows_x64\xv.loc\ru.ecst
estrtab xv_com\locale\en.txt :binary xv_release\_build\windows_x64\xv.loc\en.ecst
estrtab xi_tool\locale\ru.txt :binary xv_release\_build\windows_x64\xi.loc\ru.ecst
estrtab xi_tool\locale\en.txt :binary xv_release\_build\windows_x64\xi.loc\en.ecst