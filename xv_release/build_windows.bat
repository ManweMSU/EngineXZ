@echo off
ertbuild xv_com\xv.ertproj -Nar %ARCH%
ertbuild xv_sl\xvsl.ertproj -Nar %ARCH%
ertbuild xv_mm\xvm.ertproj -Nar %ARCH%
ertbuild xi_tool\xi.ertproj -Nar %ARCH%
ertbuild xi_dasm\xda.ertproj -Nar %ARCH%
mkdir xv_release\_build
del /S /Q xv_release\_build\windows_%ARCH%\*
mkdir xv_release\_build\windows_%ARCH%
mkdir xv_release\_build\com
copy /B xv_mm\_build\windows_%ARCH%_release\xvm.exe xv_release\_build\windows_%ARCH%\xvm.exe
mkdir xv_release\_build\windows_%ARCH%\xvcl
mkdir xv_release\_build\windows_%ARCH%\vscx
mkdir xv_release\_build\windows_%ARCH%\xv.loc
mkdir xv_release\_build\windows_%ARCH%\xi.loc
mkdir xv_release\_build\windows_%ARCH%\xda.loc
copy /B xv_com\xv.ini xv_release\_build\windows_%ARCH%\xv.ini
copy /B xv_com\xe.ini xv_release\_build\windows_%ARCH%\xe.ini
copy /B xi_tool\xi.ini xv_release\_build\windows_%ARCH%\xi.ini
copy /B xi_dasm\xda.ini xv_release\_build\windows_%ARCH%\xda.ini
copy /B xv_com\_build\windows_%ARCH%_release\xv.exe xv_release\_build\windows_%ARCH%\xv.exe
copy /B xv_sl\_build\windows_%ARCH%_release\xvsl.exe xv_release\_build\windows_%ARCH%\xvsl.exe
copy /B xi_tool\_build\windows_%ARCH%_release\xi.exe xv_release\_build\windows_%ARCH%\xi.exe
copy /B xi_dasm\_build\windows_%ARCH%_release\xda.exe xv_release\_build\windows_%ARCH%\xda.exe
copy /B xv_release\engine-xv-vscx-1.0.0.vsix xv_release\_build\windows_%ARCH%\vscx\xv-vscx.vsix
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\canonicalis.xv     -NPVom  xv_release\_build\com\canonicalis.xo    xv_lib\canonicalis.xvm
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\limae.xv           -NVoml  xv_release\_build\com\limae.xo          xv_lib\limae.xvm            xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\imago.xv           -NVoml  xv_release\_build\com\imago.xo          xv_lib\imago.xvm            xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\consolatorium.xv   -NVoml  xv_release\_build\com\consolatorium.xo  xv_lib\consolatorium.xvm    xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\formati.xv         -NVoml  xv_release\_build\com\formati.xo        xv_lib\formati.xvm          xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\lxx.xv             -NVoml  xv_release\_build\com\lxx.xo            xv_lib\lxx.xvm              xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\potentia.xv        -NVoml  xv_release\_build\com\potentia.xo       xv_lib\potentia.xvm         xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\fenestrae.xv       -NVoml  xv_release\_build\com\fenestrae.xo      xv_lib\fenestrae.xvm        xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\graphicum.xv       -NVoml  xv_release\_build\com\graphicum.xo      xv_lib\graphicum.xvm        xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\repulsus.xv        -NVoml  xv_release\_build\com\repulsus.xo       xv_lib\repulsus.xvm         xv_release\_build\com
xcopy /BY xv_release\_build\com\* xv_release\_build\windows_%ARCH%\xvcl\
xcopy /BY xv_lib\*.xvm xv_release\_build\windows_%ARCH%\xvcl\
estrtab xv_com\locale\ru.txt :binary xv_release\_build\windows_%ARCH%\xv.loc\ru.ecst
estrtab xv_com\locale\en.txt :binary xv_release\_build\windows_%ARCH%\xv.loc\en.ecst
estrtab xi_tool\locale\ru.txt :binary xv_release\_build\windows_%ARCH%\xi.loc\ru.ecst
estrtab xi_tool\locale\en.txt :binary xv_release\_build\windows_%ARCH%\xi.loc\en.ecst
estrtab xi_dasm\locale\ru.txt :binary xv_release\_build\windows_%ARCH%\xda.loc\ru.ecst
estrtab xi_dasm\locale\en.txt :binary xv_release\_build\windows_%ARCH%\xda.loc\en.ecst