@echo off
..\xv_release\_build\windows_x64\xv lib\cor.xv -NPVo lib\cor.xo
..\xv_release\_build\windows_x64\xv lib\winapi.xv -NPVo lib\winapi.xo
..\xv_release\_build\windows_x64\xv lib\memoria.xv -NPVo lib\memoria.xo
..\xv_release\_build\windows_x64\xv lib\lineae.xv -NPVo lib\lineae.xo
..\xv_release\_build\windows_x64\xv lib\mathcan.xv -NPVo lib\mathcan.xo
..\xv_release\_build\windows_x64\xv lib\flumeni.xv -NPVo lib\flumeni.xo
..\xv_release\_build\windows_x64\xv lib\contextus.xv -NPVo lib\contextus.xo
..\xv_release\_build\windows_x64\xv lib\errores.xv -NPVo lib\errores.xo
..\xv_release\_build\windows_x64\xv lib\canonicalis.xv -NPVo lib\canonicalis.xo
..\xv_release\_build\windows_x64\xv lib\limae.xv -NVo lib\limae.xo
..\xv_release\_build\windows_x64\xv lib\consolatorium.xv -NVo lib\consolatorium.xo

..\xv_release\_build\windows_x64\xv test.xv -No test.xx
linker\_build\windows_x64_release\xncon asm\cortex.asm -Nmo dos-exe cortex.exe
linker\_build\windows_x64_release\xncon test.xx -Nbmol cortex.exe win-x64 x64.exe lib
linker\_build\windows_x64_release\xncon test.xx -Nbmol cortex.exe win-x86 x86.exe lib
linker\_build\windows_x64_release\xncon test.xx -Nbmol cortex.exe win-arm64 arm64.exe lib