ertbuild xv_com/xv.ertproj -Nar ${ARCH}
ertbuild xw_dec/xw.ertproj -Nar ${ARCH}
ertbuild xw_pc/xwpc.ertproj -Nar ${ARCH}
ertbuild xv_sl/xvsl.ertproj -Nar ${ARCH}
ertbuild xv_mm/xvm.ertproj -Nar ${ARCH}
ertbuild xi_tool/xi.ertproj -Nar ${ARCH}
ertbuild xi_dasm/xda.ertproj -Nar ${ARCH}
mkdir xv_release/_build
rm -r xv_release/_build/macosx_${ARCH}
mkdir xv_release/_build/macosx_${ARCH}
mkdir xv_release/_build/com
mkdir xv_release/_build/comw
cp -R xv_mm/_build/macosx_${ARCH}_release/xvm.app xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app
mkdir xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl
mkdir xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xwcl
mkdir xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/vscx
mkdir xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv.loc
mkdir xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xw.loc
mkdir xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi.loc
mkdir xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xda.loc
cp xv_com/xv.ini xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv.ini
cp xv_com/xe.ini xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xe.ini
cp xw_dec/xw.ini xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xw.ini
cp xi_tool/xi.ini xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi.ini
cp xi_dasm/xda.ini xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xda.ini
cp xv_com/_build/macosx_${ARCH}_release/xv xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv
cp xw_dec/_build/macosx_${ARCH}_release/xw xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xw
cp xw_pc/_build/macosx_${ARCH}_release/xwpc xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xwpc
cp xv_sl/_build/macosx_${ARCH}_release/xvsl xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvsl
cp xi_tool/_build/macosx_${ARCH}_release/xi xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi
cp xi_dasm/_build/macosx_${ARCH}_release/xda xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xda
cp xv_release/engine-xv-vscx-1.0.0.vsix xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/vscx/xv-vscx.vsix
cp xv_lib/manualis.xvm xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl/
cp xv_lib/canonicalis.xvm xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl/
cp xv_lib/limae.xvm xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl/
cp xv_lib/imago.xvm xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl/
cp xv_lib/consolatorium.xvm xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl/
cp xv_lib/formati.xvm xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl/
cp xv_lib/lxx.xvm xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl/
cp xv_lib/potentia.xvm xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl/
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/canonicalis.xv         -NPVo xv_release/_build/com/canonicalis.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/limae.xv               -NVol xv_release/_build/com/limae.xo               xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/imago.xv               -NVol xv_release/_build/com/imago.xo               xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/consolatorium.xv       -NVol xv_release/_build/com/consolatorium.xo       xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/formati.xv             -NVol xv_release/_build/com/formati.xo             xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/lxx.xv                 -NVol xv_release/_build/com/lxx.xo                 xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/potentia.xv            -NVol xv_release/_build/com/potentia.xo            xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/fenestrae.xv           -NVol xv_release/_build/com/fenestrae.xo           xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/graphicum.xv           -NVol xv_release/_build/com/graphicum.xo           xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/repulsus.xv            -NVol xv_release/_build/com/repulsus.xo            xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/mathvec.xv             -NVol xv_release/_build/com/mathvec.xo             xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/mathcom.xv             -NVol xv_release/_build/com/mathcom.xo             xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/mathvcom.xv            -NVol xv_release/_build/com/mathvcom.xo            xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/matrices.xv            -NVol xv_release/_build/com/matrices.xo            xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/comatrices.xv          -NVol xv_release/_build/com/comatrices.xo          xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/mathquat.xv            -NVol xv_release/_build/com/mathquat.xo            xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/mathcolorum.xv         -NVol xv_release/_build/com/mathcolorum.xo         xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/mathgraphici.xv        -NVol xv_release/_build/com/mathgraphici.xo        xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/typographica.xv        -NVol xv_release/_build/com/typographica.xo        xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/communicatio.xv        -NVol xv_release/_build/com/communicatio.xo        xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/collectiones.xv        -NVol xv_release/_build/com/collectiones.xo        xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/serializatio.xv        -NVol xv_release/_build/com/serializatio.xo        xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/xesl.xv                -NVol xv_release/_build/com/xesl.xo                xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/json.xv                -NVol xv_release/_build/com/json.xo                xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/ecso.xv                -NVol xv_release/_build/com/ecso.xo                xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/ifr.xv                 -NVol xv_release/_build/com/ifr.xo                 xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/http.xv                -NVol xv_release/_build/com/http.xo                xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/http_sessio.xv         -NVol xv_release/_build/com/http_sessio.xo         xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/http_servus.xv         -NVol xv_release/_build/com/http_servus.xo         xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/http_servus_limarum.xv -NVol xv_release/_build/com/http_servus_limarum.xo xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/commem.xv              -NVol xv_release/_build/com/commem.xo              xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/cryptographia.xv       -NVol xv_release/_build/com/cryptographia.xo       xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/subscriptiones.xv      -NVol xv_release/_build/com/subscriptiones.xo      xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/media.xv               -NVol xv_release/_build/com/media.xo               xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/audio.xv               -NVol xv_release/_build/com/audio.xo               xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/video.xv               -NVol xv_release/_build/com/video.xo               xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xw_lib/canonicalis.xw         -NVolm xv_release/_build/comw/canonicalis.xwo      xv_release/_build/comw	xw_lib/canonicalis.xvm
cp xv_lib/*.xvm xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl
cp xw_lib/*.xvm xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xwcl
cp xv_release/_build/com/*.xo xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl
cp xv_release/_build/comw/*.xwo xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xwcl
estrtab xv_com/locale/ru.txt -Nfo bin xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv.loc/ru.ecst
estrtab xv_com/locale/en.txt -Nfo bin xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv.loc/en.ecst
estrtab xw_dec/locale/ru.txt -Nfo bin xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xw.loc/ru.ecst
estrtab xw_dec/locale/en.txt -Nfo bin xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xw.loc/en.ecst
estrtab xi_tool/locale/ru.txt -Nfo bin xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi.loc/ru.ecst
estrtab xi_tool/locale/en.txt -Nfo bin xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi.loc/en.ecst
estrtab xi_dasm/locale/ru.txt -Nfo bin xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xda.loc/ru.ecst
estrtab xi_dasm/locale/en.txt -Nfo bin xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xda.loc/en.ecst