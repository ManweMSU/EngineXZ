cd ~/Documents/GitHub/EngineXZ/shellex_macos
ARCH=arm64
source thumbex.sh
ertbuild test.ertproj -N
mkdir _build/test/test.app/Contents/PlugIns
cp -r _build/macosx_${ARCH}_release/xxqlex.appex _build/test/test.app/Contents/PlugIns
open _build/test/test.app