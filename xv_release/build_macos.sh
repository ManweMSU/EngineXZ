ertbuild xv_com/xv.ertproj -Nar ${ARCH}
ertbuild xv_sl/xvsl.ertproj -Nar ${ARCH}
ertbuild xv_mm/xvm.ertproj -Nar ${ARCH}
ertbuild xi_tool/xi.ertproj -Nar ${ARCH}
ertbuild xi_dasm/xda.ertproj -Nar ${ARCH}
mkdir xv_release/_build
rm -r xv_release/_build/macosx_${ARCH}
mkdir xv_release/_build/macosx_${ARCH}
mkdir xv_release/_build/com
cp -R xv_mm/_build/macosx_${ARCH}_release/xvm.app xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app
mkdir xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl
mkdir xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/vscx
mkdir xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv.loc
mkdir xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi.loc
mkdir xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xda.loc
cp xv_com/xv.ini xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv.ini
cp xv_com/xe.ini xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xe.ini
cp xi_tool/xi.ini xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi.ini
cp xi_dasm/xda.ini xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xda.ini
cp xv_com/_build/macosx_${ARCH}_release/xv xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv
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
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/canonicalis.xv      -NPVo xv_release/_build/com/canonicalis.xo
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/limae.xv            -NVol xv_release/_build/com/limae.xo            xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/imago.xv            -NVol xv_release/_build/com/imago.xo            xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/consolatorium.xv    -NVol xv_release/_build/com/consolatorium.xo    xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/formati.xv          -NVol xv_release/_build/com/formati.xo          xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/lxx.xv              -NVol xv_release/_build/com/lxx.xo              xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/potentia.xv         -NVol xv_release/_build/com/potentia.xo         xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/fenestrae.xv        -NVol xv_release/_build/com/fenestrae.xo        xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/graphicum.xv        -NVol xv_release/_build/com/graphicum.xo        xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/repulsus.xv         -NVol xv_release/_build/com/repulsus.xo         xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/mathvec.xv          -NVol xv_release/_build/com/mathvec.xo          xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/mathcom.xv          -NVol xv_release/_build/com/mathcom.xo          xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/mathvcom.xv         -NVol xv_release/_build/com/mathvcom.xo         xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/matrices.xv         -NVol xv_release/_build/com/matrices.xo         xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/comatrices.xv       -NVol xv_release/_build/com/comatrices.xo       xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/mathquat.xv         -NVol xv_release/_build/com/mathquat.xo         xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/mathcolorum.xv      -NVol xv_release/_build/com/mathcolorum.xo      xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/mathgraphici.xv     -NVol xv_release/_build/com/mathgraphici.xo     xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/typographica.xv     -NVol xv_release/_build/com/typographica.xo     xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/communicatio.xv     -NVol xv_release/_build/com/communicatio.xo     xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/collectiones.xv     -NVol xv_release/_build/com/collectiones.xo     xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/serializatio.xv     -NVol xv_release/_build/com/serializatio.xo     xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/xesl.xv             -NVol xv_release/_build/com/xesl.xo             xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/json.xv             -NVol xv_release/_build/com/json.xo             xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/ecso.xv             -NVol xv_release/_build/com/ecso.xo             xv_release/_build/com
./xv_release/_build/macosx_${XVC_ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/ifr.xv              -NVol xv_release/_build/com/ifr.xo              xv_release/_build/com
cp xv_lib/*.xvm xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl
cp xv_release/_build/com/*.xo xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl
estrtab xv_com/locale/ru.txt :binary xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv.loc/ru.ecst
estrtab xv_com/locale/en.txt :binary xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv.loc/en.ecst
estrtab xi_tool/locale/ru.txt :binary xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi.loc/ru.ecst
estrtab xi_tool/locale/en.txt :binary xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi.loc/en.ecst
estrtab xi_dasm/locale/ru.txt :binary xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xda.loc/ru.ecst
estrtab xi_dasm/locale/en.txt :binary xv_release/_build/macosx_${ARCH}/XV\ Monstrans\ Manualis.app/Contents/MacOS/xda.loc/en.ecst