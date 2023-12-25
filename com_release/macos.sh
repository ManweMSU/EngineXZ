cd /Users/manwe/Documents/GitHub/EngineJIT
xv shellex_macos/patch.xv -N
./xv_release/build_macos_x64.sh
./xv_release/build_macos_arm64.sh
./xx_release/build_macos_x64.sh
./xx_release/build_macos_arm64.sh
rm shellex_macos/patch.xx
ecfxpack com_release/xv-macos.ecsa -d xv_release/_build/macosx_x64 macosx-x64 ''\
                                   -d xv_release/_build/macosx_arm64 macosx-arm64 ''
ecfxpack com_release/xx-macos.ecsa -d xx_release/_build/macosx_x64 macosx-x64 ''\
                                   -d xx_release/_build/macosx_arm64 macosx-arm64 ''
./purify.sh