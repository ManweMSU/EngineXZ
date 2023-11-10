cd /Users/manwe/Documents/GitHub/EngineJIT
ertbuild xv_com/xv.ertproj -Nar x64
ertbuild xv_sl/xvsl.ertproj -Nar x64
ertbuild xv_mm/xvm.ertproj -Nar x64
ertbuild xi_tool/xi.ertproj -Nar x64
mkdir xv_release/_build
rm -r xv_release/_build/macosx_x64
mkdir xv_release/_build/macosx_x64
cp -R xv_mm/_build/macosx_x64_release/xvm.app xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app
mkdir xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl
mkdir xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/vscx
mkdir xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv.loc
mkdir xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi.loc
cp xv_com/xv.ini xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv.ini
cp xv_com/xe.ini xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xe.ini
cp xi_tool/xi.ini xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi.ini
cp xv_com/_build/macosx_x64_release/xv xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv
cp xv_sl/_build/macosx_x64_release/xvsl xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvsl
cp xi_tool/_build/macosx_x64_release/xi xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi
cp xv_release/engine-xv-vscx-1.0.0.vsix xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/vscx/xv-vscx.vsix
cp xv_lib/manualis.xvm xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl/
cp xv_lib/canonicalis.xvm xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl/
cp xv_lib/limae.xvm xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl/
cp xv_lib/imago.xvm xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl/
cp xv_lib/consolatorium.xvm xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl/
cp xv_lib/formati.xvm xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl/
cp xv_lib/lxx.xvm xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl/
./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/canonicalis.xv -NO xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl
./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/limae.xv -NO xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl
./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/imago.xv -NO xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl
./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/consolatorium.xv -NO xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl
./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/formati.xv -NO xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl
./xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv xv_lib/lxx.xv -NO xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xvcl
estrtab xv_com/locale/ru.txt :binary xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv.loc/ru.ecst
estrtab xv_com/locale/en.txt :binary xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xv.loc/en.ecst
estrtab xi_tool/locale/ru.txt :binary xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi.loc/ru.ecst
estrtab xi_tool/locale/en.txt :binary xv_release/_build/macosx_x64/XV\ Monstrans\ Manualis.app/Contents/MacOS/xi.loc/en.ecst