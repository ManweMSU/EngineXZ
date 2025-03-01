@echo off
ertbuild xv_com\xv.ertproj -Nar %ARCH%
ertbuild xw_dec\xw.ertproj -Nar %ARCH%
ertbuild xv_sl\xvsl.ertproj -Nar %ARCH%
ertbuild xv_mm\xvm.ertproj -Nar %ARCH%
ertbuild xi_tool\xi.ertproj -Nar %ARCH%
ertbuild xi_dasm\xda.ertproj -Nar %ARCH%
mkdir xv_release\_build
del /S /Q xv_release\_build\windows_%ARCH%\*
mkdir xv_release\_build\windows_%ARCH%
mkdir xv_release\_build\com
mkdir xv_release\_build\comw
copy /B xv_mm\_build\windows_%ARCH%_release\xvm.exe xv_release\_build\windows_%ARCH%\xvm.exe
mkdir xv_release\_build\windows_%ARCH%\xvcl
mkdir xv_release\_build\windows_%ARCH%\xwcl
mkdir xv_release\_build\windows_%ARCH%\vscx
mkdir xv_release\_build\windows_%ARCH%\xv.loc
mkdir xv_release\_build\windows_%ARCH%\xw.loc
mkdir xv_release\_build\windows_%ARCH%\xi.loc
mkdir xv_release\_build\windows_%ARCH%\xda.loc
copy /B xv_com\xv.ini xv_release\_build\windows_%ARCH%\xv.ini
copy /B xv_com\xe.ini xv_release\_build\windows_%ARCH%\xe.ini
copy /B xw_dec\xw.ini xv_release\_build\windows_%ARCH%\xw.ini
copy /B xi_tool\xi.ini xv_release\_build\windows_%ARCH%\xi.ini
copy /B xi_dasm\xda.ini xv_release\_build\windows_%ARCH%\xda.ini
copy /B xv_com\_build\windows_%ARCH%_release\xv.exe xv_release\_build\windows_%ARCH%\xv.exe
copy /B xw_dec\_build\windows_%ARCH%_release\xw.exe xv_release\_build\windows_%ARCH%\xw.exe
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
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\mathvec.xv         -NVoml  xv_release\_build\com\mathvec.xo        xv_lib\mathvec.xvm          xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\mathcom.xv         -NVoml  xv_release\_build\com\mathcom.xo        xv_lib\mathcom.xvm          xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\mathvcom.xv        -NVoml  xv_release\_build\com\mathvcom.xo       xv_lib\mathvcom.xvm         xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\matrices.xv        -NVoml  xv_release\_build\com\matrices.xo       xv_lib\matrices.xvm         xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\comatrices.xv      -NVoml  xv_release\_build\com\comatrices.xo     xv_lib\comatrices.xvm       xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\mathquat.xv        -NVoml  xv_release\_build\com\mathquat.xo       xv_lib\mathquat.xvm         xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\mathcolorum.xv     -NVoml  xv_release\_build\com\mathcolorum.xo    xv_lib\mathcolorum.xvm      xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\mathgraphici.xv    -NVoml  xv_release\_build\com\mathgraphici.xo   xv_lib\mathgraphici.xvm     xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\typographica.xv    -NVoml  xv_release\_build\com\typographica.xo   xv_lib\typographica.xvm     xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\communicatio.xv    -NVoml  xv_release\_build\com\communicatio.xo   xv_lib\communicatio.xvm     xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\collectiones.xv    -NVoml  xv_release\_build\com\collectiones.xo   xv_lib\collectiones.xvm     xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\serializatio.xv    -NVoml  xv_release\_build\com\serializatio.xo   xv_lib\serializatio.xvm     xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\xesl.xv            -NVoml  xv_release\_build\com\xesl.xo           xv_lib\xesl.xvm             xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\json.xv            -NVoml  xv_release\_build\com\json.xo           xv_lib\json.xvm             xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\ecso.xv            -NVoml  xv_release\_build\com\ecso.xo           xv_lib\ecso.xvm             xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\ifr.xv             -NVoml  xv_release\_build\com\ifr.xo            xv_lib\ifr.xvm              xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\http.xv            -NVoml  xv_release\_build\com\http.xo           xv_lib\http.xvm             xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\commem.xv          -NVoml  xv_release\_build\com\commem.xo         xv_lib\commem.xvm           xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\cryptographia.xv   -NVoml  xv_release\_build\com\cryptographia.xo  xv_lib\cryptographia.xvm    xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\media.xv           -NVoml  xv_release\_build\com\media.xo          xv_lib\media.xvm            xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\audio.xv           -NVoml  xv_release\_build\com\audio.xo          xv_lib\audio.xvm            xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xv_lib\video.xv           -NVoml  xv_release\_build\com\video.xo          xv_lib\video.xvm            xv_release\_build\com
.\xv_release\_build\windows_%XVC_ARCH%\xv.exe xw_lib\canonicalis.xw     -NPVom  xv_release\_build\comw\canonicalis.xwo  xw_lib\canonicalis.xvm
xcopy /BY xv_release\_build\com\* xv_release\_build\windows_%ARCH%\xvcl\
xcopy /BY xv_release\_build\comw\* xv_release\_build\windows_%ARCH%\xwcl\
xcopy /BY xv_lib\*.xvm xv_release\_build\windows_%ARCH%\xvcl\
xcopy /BY xw_lib\*.xvm xv_release\_build\windows_%ARCH%\xwcl\
estrtab xv_com\locale\ru.txt -Nfo bin xv_release\_build\windows_%ARCH%\xv.loc\ru.ecst
estrtab xv_com\locale\en.txt -Nfo bin xv_release\_build\windows_%ARCH%\xv.loc\en.ecst
estrtab xw_dec\locale\ru.txt -Nfo bin xv_release\_build\windows_%ARCH%\xw.loc\ru.ecst
estrtab xw_dec\locale\en.txt -Nfo bin xv_release\_build\windows_%ARCH%\xw.loc\en.ecst
estrtab xi_tool\locale\ru.txt -Nfo bin xv_release\_build\windows_%ARCH%\xi.loc\ru.ecst
estrtab xi_tool\locale\en.txt -Nfo bin xv_release\_build\windows_%ARCH%\xi.loc\en.ecst
estrtab xi_dasm\locale\ru.txt -Nfo bin xv_release\_build\windows_%ARCH%\xda.loc\ru.ecst
estrtab xi_dasm\locale\en.txt -Nfo bin xv_release\_build\windows_%ARCH%\xda.loc\en.ecst