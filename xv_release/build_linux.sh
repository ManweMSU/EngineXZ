esse xv_com/xv.ertproj -Nar ${ARCH}
esse xw_dec/xw.ertproj -Nar ${ARCH}
esse xw_pc/xwpc.ertproj -Nar ${ARCH}
esse xv_sl/xvsl.ertproj -Nar ${ARCH}
esse xi_tool/xi.ertproj -Nar ${ARCH}
esse xi_dasm/xda.ertproj -Nar ${ARCH}
esse x_uicc/xuicc.ertproj -Nar ${ARCH}
# esse xv_mm/xvm.ertproj -Nar ${ARCH}
mkdir xv_release/_build
rm -r xv_release/_build/linux_${ARCH}
mkdir xv_release/_build/linux_${ARCH}
mkdir xv_release/_build/com
mkdir xv_release/_build/comw
# COPY XVMM
mkdir xv_release/_build/linux_${ARCH}/xvcl
mkdir xv_release/_build/linux_${ARCH}/xwcl
mkdir xv_release/_build/linux_${ARCH}/xuil
mkdir xv_release/_build/linux_${ARCH}/vscx
mkdir xv_release/_build/linux_${ARCH}/xv.loc
mkdir xv_release/_build/linux_${ARCH}/xw.loc
mkdir xv_release/_build/linux_${ARCH}/xi.loc
mkdir xv_release/_build/linux_${ARCH}/xda.loc
mkdir xv_release/_build/linux_${ARCH}/xuicc.loc
cp xv_com/xv.ini xv_release/_build/linux_${ARCH}/xv.ini
cp xv_com/xe.ini xv_release/_build/linux_${ARCH}/xe.ini
cp xw_dec/xw.ini xv_release/_build/linux_${ARCH}/xw.ini
cp xi_tool/xi.ini xv_release/_build/linux_${ARCH}/xi.ini
cp xi_dasm/xda.ini xv_release/_build/linux_${ARCH}/xda.ini
cp x_uicc/xuicc.ini xv_release/_build/linux_${ARCH}/xuicc.ini
cp xv_com/_build/linux_${ARCH}_release/xv xv_release/_build/linux_${ARCH}/xv
cp xw_dec/_build/linux_${ARCH}_release/xw xv_release/_build/linux_${ARCH}/xw
cp xw_pc/_build/linux_${ARCH}_release/xwpc xv_release/_build/linux_${ARCH}/xwpc
cp xv_sl/_build/linux_${ARCH}_release/xvsl xv_release/_build/linux_${ARCH}/xvsl
cp xi_tool/_build/linux_${ARCH}_release/xi xv_release/_build/linux_${ARCH}/xi
cp xi_dasm/_build/linux_${ARCH}_release/xda xv_release/_build/linux_${ARCH}/xda
cp x_uicc/_build/linux_${ARCH}_release/xuicc xv_release/_build/linux_${ARCH}/xuicc
cp xv_release/engine-xv-vscx-1.0.0.vsix xv_release/_build/linux_${ARCH}/vscx/xv-vscx.vsix

./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/canonicalis.xv         	-NPVo xv_release/_build/com/canonicalis.xo
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/limae.xv               	-NVol xv_release/_build/com/limae.xo               	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/imago.xv               	-NVol xv_release/_build/com/imago.xo               	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/consolatorium.xv       	-NVol xv_release/_build/com/consolatorium.xo       	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/formati.xv             	-NVol xv_release/_build/com/formati.xo             	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/lxx.xv                 	-NVol xv_release/_build/com/lxx.xo                 	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/potentia.xv            	-NVol xv_release/_build/com/potentia.xo            	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/fenestrae.xv           	-NVol xv_release/_build/com/fenestrae.xo           	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/graphicum.xv           	-NVol xv_release/_build/com/graphicum.xo           	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/repulsus.xv            	-NVol xv_release/_build/com/repulsus.xo            	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/mathvec.xv             	-NVol xv_release/_build/com/mathvec.xo             	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/mathcom.xv             	-NVol xv_release/_build/com/mathcom.xo             	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/mathvcom.xv            	-NVol xv_release/_build/com/mathvcom.xo            	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/matrices.xv            	-NVol xv_release/_build/com/matrices.xo            	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/comatrices.xv          	-NVol xv_release/_build/com/comatrices.xo          	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/mathquat.xv            	-NVol xv_release/_build/com/mathquat.xo            	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/mathcolorum.xv         	-NVol xv_release/_build/com/mathcolorum.xo         	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/mathgraphici.xv        	-NVol xv_release/_build/com/mathgraphici.xo        	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/typographica.xv        	-NVol xv_release/_build/com/typographica.xo        	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/communicatio.xv        	-NVol xv_release/_build/com/communicatio.xo        	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/collectiones.xv        	-NVol xv_release/_build/com/collectiones.xo        	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/serializatio.xv        	-NVol xv_release/_build/com/serializatio.xo        	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/xesl.xv                	-NVol xv_release/_build/com/xesl.xo                	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/json.xv                	-NVol xv_release/_build/com/json.xo                	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/ecso.xv                	-NVol xv_release/_build/com/ecso.xo                	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/ifr.xv                 	-NVol xv_release/_build/com/ifr.xo                 	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/http.xv                	-NVol xv_release/_build/com/http.xo                	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/http_sessio.xv         	-NVol xv_release/_build/com/http_sessio.xo         	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/http_servus.xv         	-NVol xv_release/_build/com/http_servus.xo         	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/http_servus_limarum.xv 	-NVol xv_release/_build/com/http_servus_limarum.xo 	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/commem.xv              	-NVol xv_release/_build/com/commem.xo              	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/cryptographia.xv       	-NVol xv_release/_build/com/cryptographia.xo       	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/subscriptiones.xv      	-NVol xv_release/_build/com/subscriptiones.xo      	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/media.xv               	-NVol xv_release/_build/com/media.xo               	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/audio.xv               	-NVol xv_release/_build/com/audio.xo               	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/video.xv               	-NVol xv_release/_build/com/video.xo               	xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/integri_largi.xv        -NVol xv_release/_build/com/integri_largi.xo        xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/cryptographia_nativa.xv -NVol xv_release/_build/com/cryptographia_nativa.xo xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/ingenium_iu.xv 			-NVol xv_release/_build/com/ingenium_iu.xo 			xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xv_lib/ingenium_iu_ext.xv 		-NVol xv_release/_build/com/ingenium_iu_ext.xo 		xv_release/_build/com
./xv_release/_build/linux_${XVC_ARCH}/xv xw_lib/canonicalis.xw         	-NVolm xv_release/_build/comw/canonicalis.xwo      	xv_release/_build/comw	xw_lib/canonicalis.xvm
cp xv_lib/*.xvm xv_release/_build/linux_${ARCH}/xvcl
cp xw_lib/*.xvm xv_release/_build/linux_${ARCH}/xwcl
cp xv_release/_build/com/*.xo xv_release/_build/linux_${ARCH}/xvcl
cp xv_release/_build/comw/*.xwo xv_release/_build/linux_${ARCH}/xwcl
esse-converte lineae xv_com/locale/ru.txt -Nfo bin xv_release/_build/linux_${ARCH}/xv.loc/ru.ecst
esse-converte lineae xv_com/locale/en.txt -Nfo bin xv_release/_build/linux_${ARCH}/xv.loc/en.ecst
esse-converte lineae xw_dec/locale/ru.txt -Nfo bin xv_release/_build/linux_${ARCH}/xw.loc/ru.ecst
esse-converte lineae xw_dec/locale/en.txt -Nfo bin xv_release/_build/linux_${ARCH}/xw.loc/en.ecst
esse-converte lineae xi_tool/locale/ru.txt -Nfo bin xv_release/_build/linux_${ARCH}/xi.loc/ru.ecst
esse-converte lineae xi_tool/locale/en.txt -Nfo bin xv_release/_build/linux_${ARCH}/xi.loc/en.ecst
esse-converte lineae xi_dasm/locale/ru.txt -Nfo bin xv_release/_build/linux_${ARCH}/xda.loc/ru.ecst
esse-converte lineae xi_dasm/locale/en.txt -Nfo bin xv_release/_build/linux_${ARCH}/xda.loc/en.ecst
esse-converte lineae x_uicc/locale/ru.txt -Nfo bin xv_release/_build/linux_${ARCH}/xuicc.loc/ru.ecst
esse-converte lineae x_uicc/locale/en.txt -Nfo bin xv_release/_build/linux_${ARCH}/xuicc.loc/en.ecst