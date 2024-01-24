cd /Users/manwe/Documents/GitHub/EngineJIT
ertbuild xcon/xc.ertproj -Nar arm64
ertbuild xx_xx/xx.ertproj -Nar arm64
ertbuild xx_xxf/xxf.ertproj -Nar arm64
ertbuild xx_xxsc/xxsc.ertproj -Nar arm64

mkdir xx_release/_build
mkdir xx_release/_build/com
rm -rf xx_release/_build/macosx_arm64
mkdir xx_release/_build/macosx_arm64

cp -R xx_xxsc/_build/macosx_arm64_release/xxsc.app xx_release/_build/macosx_arm64/XX.app
cp -R xx_xxf/_build/macosx_arm64_release/xxf.app xx_release/_build/macosx_arm64/XX.app/XX.app
cp -R xcon/_build/macosx_arm64_release/xc.app xx_release/_build/macosx_arm64/XX.app/XC.app
mkdir xx_release/_build/macosx_arm64/XX.app/xxcl
mkdir xx_release/_build/macosx_arm64/XX.app/xxi
mkdir xx_release/_build/macosx_arm64/XX.app/xx
cp xx_xx/xx_mac_f.ini xx_release/_build/macosx_arm64/XX.app/XX.app/Contents/MacOS/xx.ini
cp xx_xx/_build/macosx_arm64_release/xx xx_release/_build/macosx_arm64/XX.app/xx/xx
cp xx_xx/xx_mac.ini xx_release/_build/macosx_arm64/XX.app/xx/xx.ini
cp xx_xx/xe.ini xx_release/_build/macosx_arm64/XX.app/xe.ini

./shellex_macos/assembly.sh
xx shellex_macos/patch.xx xx_release/_build/macosx_arm64/XX.app shellex_macos/_build/plugin/XX-arm64.qlgenerator
xx shellex_macos/patch.xx xx_release/_build/macosx_arm64/XX.app/XX.app

./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/canonicalis.xv -NOP xx_release/_build/com
./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/limae.xv -NO xx_release/_build/com
./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/imago.xv -NO xx_release/_build/com
./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/consolatorium.xv -NO xx_release/_build/com
./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/lxx.xv -NO xx_release/_build/com
./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/formati.xv -NO xx_release/_build/com
./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/potentia.xv -NO xx_release/_build/com
./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/errores.ru.xv -NO xx_release/_build/com
./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/errores.en.xv -NO xx_release/_build/com
./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xx_xx/xx.xv -NOl xx_release/_build/com xx_release/_build/com

./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xx_release/_build/com/canonicalis.xo -Nto mac-arm64 xx_release/_build/macosx_arm64/XX.app/xxcl/canonicalis.xo
./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xx_release/_build/com/limae.xo -Nto mac-arm64 xx_release/_build/macosx_arm64/XX.app/xxcl/limae.xo
./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xx_release/_build/com/imago.xo -Nto mac-arm64 xx_release/_build/macosx_arm64/XX.app/xxcl/imago.xo
./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xx_release/_build/com/consolatorium.xo -Nto mac-arm64 xx_release/_build/macosx_arm64/XX.app/xxcl/consolatorium.xo
./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xx_release/_build/com/lxx.xo -Nto mac-arm64 xx_release/_build/macosx_arm64/XX.app/xxcl/lxx.xo
./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xx_release/_build/com/formati.xo -Nto mac-arm64 xx_release/_build/macosx_arm64/XX.app/xxcl/formati.xo
./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xx_release/_build/com/potentia.xo -Nto mac-arm64 xx_release/_build/macosx_arm64/XX.app/xxcl/potentia.xo
./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xx_release/_build/com/errores.ru.xo -Nto mac-arm64 xx_release/_build/macosx_arm64/XX.app/xxcl/errores.ru.xo
./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xx_release/_build/com/errores.en.xo -Nto mac-arm64 xx_release/_build/macosx_arm64/XX.app/xxcl/errores.en.xo
./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xx_release/_build/com/xx.xx -Nto mac-arm64 xx_release/_build/macosx_arm64/XX.app/xxi/xx.xx