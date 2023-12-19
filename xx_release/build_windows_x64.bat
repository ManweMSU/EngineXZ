@echo off
ertbuild xcon\xc.ertproj -Nar x64
ertbuild xx_xx\xx.ertproj -Nar x64
ertbuild xx_xxf\xxf.ertproj -Nar x64
ertbuild xx_xxsc\xxsc.ertproj -Nar x64
mkdir xx_release\_build
mkdir xx_release\_build\com
del /S /Q xx_release\_build\windows_x64\*
mkdir xx_release\_build\windows_x64
mkdir xx_release\_build\windows_x64\xc
mkdir xx_release\_build\windows_x64\xx
mkdir xx_release\_build\windows_x64\xxcl
mkdir xx_release\_build\windows_x64\xxi
copy /B xcon\_build\windows_x64_release\xc.exe xx_release\_build\windows_x64\xc\xc.exe
copy /B xcon\_build\windows_x64_release\ertwndfx.dll xx_release\_build\windows_x64\xc\ertwndfx.dll
copy /B xcon\xc.ini xx_release\_build\windows_x64\xc\xc.ini
copy /B xx_xx\_build\windows_x64_release\xx.exe xx_release\_build\windows_x64\xx\xx.exe
copy /B xx_xx\xx_win.ini xx_release\_build\windows_x64\xx\xx.ini
copy /B xx_xx\xe.ini xx_release\_build\windows_x64\xe.ini
copy /B xx_xxf\_build\windows_x64_release\xxf.exe xx_release\_build\windows_x64\xx\xxf.exe
copy /B xx_xxf\_build\windows_x64_release\ertwndfx.dll xx_release\_build\windows_x64\xx\ertwndfx.dll
copy /B xx_xxsc\_build\windows_x64_release\xxsc.exe xx_release\_build\windows_x64\xxsc.exe

xv xv_lib\canonicalis.xv -NO xx_release\_build\com
xv xv_lib\limae.xv -NO xx_release\_build\com
xv xv_lib\imago.xv -NO xx_release\_build\com
xv xv_lib\consolatorium.xv -NO xx_release\_build\com
xv xv_lib\lxx.xv -NO xx_release\_build\com
xv xv_lib\formati.xv -NO xx_release\_build\com
xv xv_lib\potentia.xv -NO xx_release\_build\com
xv xv_lib\errores.ru.xv -NO xx_release\_build\com
xv xv_lib\errores.en.xv -NO xx_release\_build\com
xv xx_xx\xx.xv -NOl xx_release\_build\com xx_release\_build\com

xi xx_release\_build\com\canonicalis.xo -Nto win-x64 xx_release\_build\windows_x64\xxcl\canonicalis.xo
xi xx_release\_build\com\limae.xo -Nto win-x64 xx_release\_build\windows_x64\xxcl\limae.xo
xi xx_release\_build\com\imago.xo -Nto win-x64 xx_release\_build\windows_x64\xxcl\imago.xo
xi xx_release\_build\com\consolatorium.xo -Nto win-x64 xx_release\_build\windows_x64\xxcl\consolatorium.xo
xi xx_release\_build\com\lxx.xo -Nto win-x64 xx_release\_build\windows_x64\xxcl\lxx.xo
xi xx_release\_build\com\formati.xo -Nto win-x64 xx_release\_build\windows_x64\xxcl\formati.xo
xi xx_release\_build\com\potentia.xo -Nto win-x64 xx_release\_build\windows_x64\xxcl\potentia.xo
xi xx_release\_build\com\errores.ru.xo -Nto win-x64 xx_release\_build\windows_x64\xxcl\errores.ru.xo
xi xx_release\_build\com\errores.en.xo -Nto win-x64 xx_release\_build\windows_x64\xxcl\errores.en.xo
xi xx_release\_build\com\xx.xx -Nto win-x64 xx_release\_build\windows_x64\xxi\xx.xx