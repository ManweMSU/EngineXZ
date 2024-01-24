@echo off
ertbuild xv_com\xv.ertproj -Nar x86
ertbuild xv_sl\xvsl.ertproj -Nar x86
ertbuild xv_mm\xvm.ertproj -Nar x86
ertbuild xi_tool\xi.ertproj -Nar x86
mkdir xv_release\_build
del /S /Q xv_release\_build\windows_x86\*
mkdir xv_release\_build\windows_x86
copy /B xv_mm\_build\windows_x86_release\xvm.exe xv_release\_build\windows_x86\xvm.exe
mkdir xv_release\_build\windows_x86\xvcl
mkdir xv_release\_build\windows_x86\vscx
mkdir xv_release\_build\windows_x86\xv.loc
mkdir xv_release\_build\windows_x86\xi.loc
copy /B xv_com\xv.ini xv_release\_build\windows_x86\xv.ini
copy /B xv_com\xe.ini xv_release\_build\windows_x86\xe.ini
copy /B xi_tool\xi.ini xv_release\_build\windows_x86\xi.ini
copy /B xv_com\_build\windows_x86_release\xv.exe xv_release\_build\windows_x86\xv.exe
copy /B xv_sl\_build\windows_x86_release\xvsl.exe xv_release\_build\windows_x86\xvsl.exe
copy /B xi_tool\_build\windows_x86_release\xi.exe xv_release\_build\windows_x86\xi.exe
copy /B xv_release\engine-xv-vscx-1.0.0.vsix xv_release\_build\windows_x86\vscx\xv-vscx.vsix
copy /B xv_lib\manualis.xvm xv_release\_build\windows_x86\xvcl\
copy /B xv_lib\canonicalis.xvm xv_release\_build\windows_x86\xvcl\
copy /B xv_lib\limae.xvm xv_release\_build\windows_x86\xvcl\
copy /B xv_lib\imago.xvm xv_release\_build\windows_x86\xvcl\
copy /B xv_lib\consolatorium.xvm xv_release\_build\windows_x86\xvcl\
copy /B xv_lib\formati.xvm xv_release\_build\windows_x86\xvcl\
copy /B xv_lib\lxx.xvm xv_release\_build\windows_x86\xvcl\
copy /B xv_lib\potentia.xvm xv_release\_build\windows_x86\xvcl\
.\xv_release\_build\windows_x86\xv.exe xv_lib\canonicalis.xv -NOPm xv_release\_build\windows_x86\xvcl xv_lib\canonicalis.xvm
.\xv_release\_build\windows_x86\xv.exe xv_lib\limae.xv -NOm xv_release\_build\windows_x86\xvcl xv_lib\limae.xvm
.\xv_release\_build\windows_x86\xv.exe xv_lib\imago.xv -NOm xv_release\_build\windows_x86\xvcl xv_lib\imago.xvm
.\xv_release\_build\windows_x86\xv.exe xv_lib\consolatorium.xv -NOm xv_release\_build\windows_x86\xvcl xv_lib\consolatorium.xvm
.\xv_release\_build\windows_x86\xv.exe xv_lib\formati.xv -NOm xv_release\_build\windows_x86\xvcl xv_lib\formati.xvm
.\xv_release\_build\windows_x86\xv.exe xv_lib\lxx.xv -NOm xv_release\_build\windows_x86\xvcl xv_lib\lxx.xvm
.\xv_release\_build\windows_x86\xv.exe xv_lib\potentia.xv -NOm xv_release\_build\windows_x86\xvcl xv_lib\potentia.xvm
estrtab xv_com\locale\ru.txt :binary xv_release\_build\windows_x86\xv.loc\ru.ecst
estrtab xv_com\locale\en.txt :binary xv_release\_build\windows_x86\xv.loc\en.ecst
estrtab xi_tool\locale\ru.txt :binary xv_release\_build\windows_x86\xi.loc\ru.ecst
estrtab xi_tool\locale\en.txt :binary xv_release\_build\windows_x86\xi.loc\en.ecst