@echo off
ertbuild xcon\xc.ertproj -Nar %ARCH%
ertbuild xx_xx\xx.ertproj -Nar %ARCH%
ertbuild xx_xxf\xxf.ertproj -Nar %ARCH%
ertbuild xx_xxsc\xxsc.ertproj -Nar %ARCH%
ertbuild shellex_windows\extension\xxcomex.ertproj -Nar %ARCH%
ertbuild shellex_windows\installer\es_installer.ertproj -Nar %ARCH%
mkdir xx_release\_build
mkdir xx_release\_build\com
del /S /Q xx_release\_build\windows_%ARCH%\*
mkdir xx_release\_build\windows_%ARCH%
mkdir xx_release\_build\windows_%ARCH%\xc
mkdir xx_release\_build\windows_%ARCH%\xx
mkdir xx_release\_build\windows_%ARCH%\xxcl
mkdir xx_release\_build\windows_%ARCH%\xxi

copy /B xcon\_build\windows_%ARCH%_release\xc.exe xx_release\_build\windows_%ARCH%\xc\xc.exe
copy /B xcon\_build\windows_%ARCH%_release\ertwndfx.dll xx_release\_build\windows_%ARCH%\xc\ertwndfx.dll
copy /B xcon\xc.ini xx_release\_build\windows_%ARCH%\xc\xc.ini
copy /B xx_xx\_build\windows_%ARCH%_release\xx.exe xx_release\_build\windows_%ARCH%\xx\xx.exe
copy /B xx_xx\xx_win.ini xx_release\_build\windows_%ARCH%\xx\xx.ini
copy /B xx_xx\xe.ini xx_release\_build\windows_%ARCH%\xe.ini
copy /B xx_xxf\_build\windows_%ARCH%_release\xxf.exe xx_release\_build\windows_%ARCH%\xx\xxf.exe
copy /B xx_xxf\_build\windows_%ARCH%_release\ertwndfx.dll xx_release\_build\windows_%ARCH%\xx\ertwndfx.dll
copy /B xx_xxsc\_build\windows_%ARCH%_release\xxsc.exe xx_release\_build\windows_%ARCH%\xxsc.exe
copy /B shellex_windows\extension\_build\windows_%ARCH%_release\xxcomex.dll xx_release\_build\windows_%ARCH%\xxcomex.dll
copy /B shellex_windows\installer\_build\windows_%ARCH%_release\es_installer.exe xx_release\_build\windows_%ARCH%\es_installer.exe

xv_release\_build\windows_%XVC_ARCH%\xv xv_lib\errores.ru.xv    -NVo xx_release\_build\com\errores.ru.xo
xv_release\_build\windows_%XVC_ARCH%\xv xv_lib\errores.en.xv    -NVo xx_release\_build\com\errores.en.xo
xv_release\_build\windows_%XVC_ARCH%\xv xx_xx\xx.xv             -NVo xx_release\_build\com\xx.xx

xv_release\_build\windows_%XVC_ARCH%\xi xv_release\_build\com\canonicalis.xo    -Nto win-%ARCH% xx_release\_build\windows_%ARCH%\xxcl\canonicalis.xo
xv_release\_build\windows_%XVC_ARCH%\xi xv_release\_build\com\limae.xo          -Nto win-%ARCH% xx_release\_build\windows_%ARCH%\xxcl\limae.xo
xv_release\_build\windows_%XVC_ARCH%\xi xv_release\_build\com\imago.xo          -Nto win-%ARCH% xx_release\_build\windows_%ARCH%\xxcl\imago.xo
xv_release\_build\windows_%XVC_ARCH%\xi xv_release\_build\com\consolatorium.xo  -Nto win-%ARCH% xx_release\_build\windows_%ARCH%\xxcl\consolatorium.xo
xv_release\_build\windows_%XVC_ARCH%\xi xv_release\_build\com\lxx.xo            -Nto win-%ARCH% xx_release\_build\windows_%ARCH%\xxcl\lxx.xo
xv_release\_build\windows_%XVC_ARCH%\xi xv_release\_build\com\formati.xo        -Nto win-%ARCH% xx_release\_build\windows_%ARCH%\xxcl\formati.xo
xv_release\_build\windows_%XVC_ARCH%\xi xv_release\_build\com\potentia.xo       -Nto win-%ARCH% xx_release\_build\windows_%ARCH%\xxcl\potentia.xo
xv_release\_build\windows_%XVC_ARCH%\xi xv_release\_build\com\fenestrae.xo      -Nto win-%ARCH% xx_release\_build\windows_%ARCH%\xxcl\fenestrae.xo
xv_release\_build\windows_%XVC_ARCH%\xi xv_release\_build\com\graphicum.xo      -Nto win-%ARCH% xx_release\_build\windows_%ARCH%\xxcl\graphicum.xo
xv_release\_build\windows_%XVC_ARCH%\xi xv_release\_build\com\repulsus.xo       -Nto win-%ARCH% xx_release\_build\windows_%ARCH%\xxcl\repulsus.xo

xv_release\_build\windows_%XVC_ARCH%\xi xx_release\_build\com\errores.ru.xo     -Nto win-%ARCH% xx_release\_build\windows_%ARCH%\xxcl\errores.ru.xo
xv_release\_build\windows_%XVC_ARCH%\xi xx_release\_build\com\errores.en.xo     -Nto win-%ARCH% xx_release\_build\windows_%ARCH%\xxcl\errores.en.xo
xv_release\_build\windows_%XVC_ARCH%\xi xx_release\_build\com\xx.xx             -Nto win-%ARCH% xx_release\_build\windows_%ARCH%\xxi\xx.xx