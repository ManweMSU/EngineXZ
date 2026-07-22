esse xcon/xc.ertproj -Nac ${ARCH} ${MODE}
esse xx_xx/xx.ertproj -Nac ${ARCH} ${MODE}
esse xx_xxf/xxf.ertproj -Nac ${ARCH} ${MODE}
esse xx_xxsc/xxsc.ertproj -Nac ${ARCH} ${MODE}

mkdir xx_release/_build
mkdir xx_release/_build/com
rm -rf xx_release/_build/linux_${ARCH}
mkdir xx_release/_build/linux_${ARCH}
mkdir xx_release/_build/linux_${ARCH}/xc
mkdir xx_release/_build/linux_${ARCH}/xx
mkdir xx_release/_build/linux_${ARCH}/xxcl
mkdir xx_release/_build/linux_${ARCH}/xxi
mkdir xx_release/_build/linux_${ARCH}/fidelitas
mkdir xx_release/_build/linux_${ARCH}/infidelitas

cp xcon/_build/linux_${ARCH}_${MODE}/xc xx_release/_build/linux_${ARCH}/xc/xc
cp xcon/xc.ini xx_release/_build/linux_${ARCH}/xc/xc.ini
cp xx_xx/_build/linux_${ARCH}_${MODE}/xx xx_release/_build/linux_${ARCH}/xx/xx
cp xx_xx/xx_lnx.ini xx_release/_build/linux_${ARCH}/xx/xx.ini
cp xx_xx/xe.ini xx_release/_build/linux_${ARCH}/xe.ini
cp xx_release/radix.xecert xx_release/_build/linux_${ARCH}/fidelitas/radix.xecert
cp xx_xxf/_build/linux_${ARCH}_${MODE}/xxf xx_release/_build/linux_${ARCH}/xx/xxf
cp xx_xxsc/_build/linux_${ARCH}_${MODE}/xxsc xx_release/_build/linux_${ARCH}/xxsc
# cp xx_xxsc/_build/linux_${ARCH}_${MODE}/xxsc.ico xx_release/_build/linux_${ARCH}/xxsc.ico
# cp xx_xxsc/_build/linux_${ARCH}_${MODE}/file_format_1.ico xx_release/_build/linux_${ARCH}/file_format_1.ico
# cp xx_xxsc/_build/linux_${ARCH}_${MODE}/file_format_2.ico xx_release/_build/linux_${ARCH}/file_format_2.ico
# cp xx_xxsc/_build/linux_${ARCH}_${MODE}/file_format_3.ico xx_release/_build/linux_${ARCH}/file_format_3.ico

./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/errores.ru.xv   -NVo xx_release/_build/com/errores.ru.xo
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/errores.en.xv   -NVo xx_release/_build/com/errores.en.xo
./xv_release/_build/linux_${XVC_ARCH}/xv xx_xx/xx.xv            -NVo xx_release/_build/com/xx.xx

./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/canonicalis.xo          -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/canonicalis.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/limae.xo                -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/limae.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/imago.xo                -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/imago.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/consolatorium.xo        -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/consolatorium.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/lxx.xo                  -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/lxx.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/formati.xo              -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/formati.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/potentia.xo             -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/potentia.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/fenestrae.xo            -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/fenestrae.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/graphicum.xo            -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/graphicum.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/repulsus.xo             -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/repulsus.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/mathvec.xo              -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/mathvec.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/mathcom.xo              -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/mathcom.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/mathvcom.xo             -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/mathvcom.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/matrices.xo             -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/matrices.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/comatrices.xo           -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/comatrices.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/mathquat.xo             -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/mathquat.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/mathcolorum.xo          -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/mathcolorum.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/mathgraphici.xo         -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/mathgraphici.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/typographica.xo         -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/typographica.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/communicatio.xo         -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/communicatio.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/collectiones.xo         -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/collectiones.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/serializatio.xo         -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/serializatio.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/xesl.xo                 -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/xesl.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/json.xo                 -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/json.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/ecso.xo                 -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/ecso.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/ifr.xo                  -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/ifr.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/http.xo                 -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/http.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/http_sessio.xo          -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/http_sessio.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/http_servus.xo          -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/http_servus.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/http_servus_limarum.xo  -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/http_servus_limarum.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/commem.xo               -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/commem.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/cryptographia.xo        -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/cryptographia.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/subscriptiones.xo       -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/subscriptiones.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/media.xo                -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/media.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/audio.xo                -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/audio.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/video.xo                -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/video.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/integri_largi.xo        -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/integri_largi.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/cryptographia_nativa.xo -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/cryptographia_nativa.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/ingenium_iu.xo          -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/ingenium_iu.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xv_release/_build/com/ingenium_iu_ext.xo      -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/ingenium_iu_ext.xo

./xv_release/_build/linux_${XVC_ARCH}/xi xx_release/_build/com/errores.ru.xo          -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/errores.ru.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xx_release/_build/com/errores.en.xo          -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxcl/errores.en.xo
./xv_release/_build/linux_${XVC_ARCH}/xi xx_release/_build/com/xx.xx                  -Nto lnx-${ARCH} xx_release/_build/linux_${ARCH}/xxi/xx.xx