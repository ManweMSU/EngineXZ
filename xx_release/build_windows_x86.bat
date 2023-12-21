@echo off
ertbuild xcon\xc.ertproj -Nar x86
ertbuild xx_xx\xx.ertproj -Nar x86
ertbuild xx_xxf\xxf.ertproj -Nar x86
ertbuild xx_xxsc\xxsc.ertproj -Nar x86
mkdir xx_release\_build
mkdir xx_release\_build\com
del /S /Q xx_release\_build\windows_x86\*
mkdir xx_release\_build\windows_x86
mkdir xx_release\_build\windows_x86\xc
mkdir xx_release\_build\windows_x86\xx
mkdir xx_release\_build\windows_x86\xxcl
mkdir xx_release\_build\windows_x86\xxi
copy /B xcon\_build\windows_x86_release\xc.exe xx_release\_build\windows_x86\xc\xc.exe
copy /B xcon\_build\windows_x86_release\ertwndfx.dll xx_release\_build\windows_x86\xc\ertwndfx.dll
copy /B xcon\xc.ini xx_release\_build\windows_x86\xc\xc.ini
copy /B xx_xx\_build\windows_x86_release\xx.exe xx_release\_build\windows_x86\xx\xx.exe
copy /B xx_xx\xx_win.ini xx_release\_build\windows_x86\xx\xx.ini
copy /B xx_xx\xe.ini xx_release\_build\windows_x86\xe.ini
copy /B xx_xxf\_build\windows_x86_release\xxf.exe xx_release\_build\windows_x86\xx\xxf.exe
copy /B xx_xxf\_build\windows_x86_release\ertwndfx.dll xx_release\_build\windows_x86\xx\ertwndfx.dll
copy /B xx_xxsc\_build\windows_x86_release\xxsc.exe xx_release\_build\windows_x86\xxsc.exe

xv_release\_build\windows_x64\xv xv_lib\canonicalis.xv -NO xx_release\_build\com
xv_release\_build\windows_x64\xv xv_lib\limae.xv -NO xx_release\_build\com
xv_release\_build\windows_x64\xv xv_lib\imago.xv -NO xx_release\_build\com
xv_release\_build\windows_x64\xv xv_lib\consolatorium.xv -NO xx_release\_build\com
xv_release\_build\windows_x64\xv xv_lib\lxx.xv -NO xx_release\_build\com
xv_release\_build\windows_x64\xv xv_lib\formati.xv -NO xx_release\_build\com
xv_release\_build\windows_x64\xv xv_lib\potentia.xv -NO xx_release\_build\com
xv_release\_build\windows_x64\xv xv_lib\errores.ru.xv -NO xx_release\_build\com
xv_release\_build\windows_x64\xv xv_lib\errores.en.xv -NO xx_release\_build\com
xv_release\_build\windows_x64\xv xx_xx\xx.xv -NOl xx_release\_build\com xx_release\_build\com

xv_release\_build\windows_x64\xi xx_release\_build\com\canonicalis.xo -Nto win-x86 xx_release\_build\windows_x86\xxcl\canonicalis.xo
xv_release\_build\windows_x64\xi xx_release\_build\com\limae.xo -Nto win-x86 xx_release\_build\windows_x86\xxcl\limae.xo
xv_release\_build\windows_x64\xi xx_release\_build\com\imago.xo -Nto win-x86 xx_release\_build\windows_x86\xxcl\imago.xo
xv_release\_build\windows_x64\xi xx_release\_build\com\consolatorium.xo -Nto win-x86 xx_release\_build\windows_x86\xxcl\consolatorium.xo
xv_release\_build\windows_x64\xi xx_release\_build\com\lxx.xo -Nto win-x86 xx_release\_build\windows_x86\xxcl\lxx.xo
xv_release\_build\windows_x64\xi xx_release\_build\com\formati.xo -Nto win-x86 xx_release\_build\windows_x86\xxcl\formati.xo
xv_release\_build\windows_x64\xi xx_release\_build\com\potentia.xo -Nto win-x86 xx_release\_build\windows_x86\xxcl\potentia.xo
xv_release\_build\windows_x64\xi xx_release\_build\com\errores.ru.xo -Nto win-x86 xx_release\_build\windows_x86\xxcl\errores.ru.xo
xv_release\_build\windows_x64\xi xx_release\_build\com\errores.en.xo -Nto win-x86 xx_release\_build\windows_x86\xxcl\errores.en.xo
xv_release\_build\windows_x64\xi xx_release\_build\com\xx.xx -Nto win-x86 xx_release\_build\windows_x86\xxi\xx.xx