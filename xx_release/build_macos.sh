ertbuild xcon/xc.ertproj -Nac ${ARCH} ${MODE}
ertbuild xx_xx/xx.ertproj -Nac ${ARCH} ${MODE}
ertbuild xx_xxf/xxf.ertproj -Nac ${ARCH} ${MODE}
ertbuild xx_xxsc/xxsc.ertproj -Nac ${ARCH} ${MODE}

mkdir xx_release/_build
mkdir xx_release/_build/com
rm -rf xx_release/_build/macosx_${ARCH}
mkdir xx_release/_build/macosx_${ARCH}

cp -R xx_xxsc/_build/macosx_${ARCH}_${MODE}/xxsc.app xx_release/_build/macosx_${ARCH}/XX.app
cp -R xx_xxf/_build/macosx_${ARCH}_${MODE}/xxf.app xx_release/_build/macosx_${ARCH}/XX.app/XX.app
cp -R xcon/_build/macosx_${ARCH}_${MODE}/xc.app xx_release/_build/macosx_${ARCH}/XX.app/XC.app
mkdir xx_release/_build/macosx_${ARCH}/XX.app/xxcl
mkdir xx_release/_build/macosx_${ARCH}/XX.app/xxi
mkdir xx_release/_build/macosx_${ARCH}/XX.app/xx
mkdir xx_release/_build/macosx_${ARCH}/XX.app/fidelitas
mkdir xx_release/_build/macosx_${ARCH}/XX.app/infidelitas
mkdir xx_release/_build/macosx_${ARCH}/XX.app/Contents/Library
mkdir xx_release/_build/macosx_${ARCH}/XX.app/Contents/Library/Spotlight
cp xx_xx/xx_mac_f.ini xx_release/_build/macosx_${ARCH}/XX.app/XX.app/Contents/MacOS/xx.ini
cp xx_xx/_build/macosx_${ARCH}_${MODE}/xx xx_release/_build/macosx_${ARCH}/XX.app/xx/xx
cp xx_xx/xx_mac.ini xx_release/_build/macosx_${ARCH}/XX.app/xx/xx.ini
cp xx_xx/xe.ini xx_release/_build/macosx_${ARCH}/XX.app/xe.ini
cp xx_release/radix.xecert xx_release/_build/macosx_${ARCH}/XX.app/fidelitas/radix.xecert

./shellex_macos/thumbex.sh ${ARCH}
./shellex_macos/xxspex.sh ${ARCH}
xv shellex_macos/patch.xv -Ndr xx_release/_build/macosx_${ARCH}/XX.app shellex_macos/_build/macosx_${ARCH}_release/xxqlex.appex
xv shellex_macos/patch.xv -Ndr xx_release/_build/macosx_${ARCH}/XX.app/XX.app
cp -r shellex_macos/_build/macosx_${ARCH}_release/xxspex.mdimporter xx_release/_build/macosx_${ARCH}/XX.app/Contents/Library/Spotlight/xxspex.mdimporter

./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/errores.ru.xv   -NVo xx_release/_build/com/errores.ru.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/errores.en.xv   -NVo xx_release/_build/com/errores.en.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xx_xx/xx.xv            -NVo xx_release/_build/com/xx.xx

./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/canonicalis.xo         -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/canonicalis.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/limae.xo               -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/limae.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/imago.xo               -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/imago.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/consolatorium.xo       -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/consolatorium.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/lxx.xo                 -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/lxx.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/formati.xo             -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/formati.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/potentia.xo            -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/potentia.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/fenestrae.xo           -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/fenestrae.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/graphicum.xo           -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/graphicum.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/repulsus.xo            -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/repulsus.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/mathvec.xo             -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/mathvec.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/mathcom.xo             -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/mathcom.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/mathvcom.xo            -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/mathvcom.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/matrices.xo            -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/matrices.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/comatrices.xo          -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/comatrices.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/mathquat.xo            -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/mathquat.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/mathcolorum.xo         -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/mathcolorum.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/mathgraphici.xo        -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/mathgraphici.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/typographica.xo        -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/typographica.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/communicatio.xo        -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/communicatio.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/collectiones.xo        -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/collectiones.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/serializatio.xo        -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/serializatio.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/xesl.xo                -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/xesl.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/json.xo                -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/json.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/ecso.xo                -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/ecso.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/ifr.xo                 -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/ifr.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/http.xo                -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/http.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/http_sessio.xo         -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/http_sessio.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/http_servus.xo         -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/http_servus.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/http_servus_limarum.xo -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/http_servus_limarum.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/commem.xo              -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/commem.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/cryptographia.xo       -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/cryptographia.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/subscriptiones.xo      -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/subscriptiones.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/media.xo               -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/media.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/audio.xo               -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/audio.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xv_release/_build/com/video.xo               -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/video.xo

./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xx_release/_build/com/errores.ru.xo          -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/errores.ru.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xx_release/_build/com/errores.en.xo          -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxcl/errores.en.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi xx_release/_build/com/xx.xx                  -Nto mac-${ARCH} xx_release/_build/macosx_${ARCH}/XX.app/xxi/xx.xx