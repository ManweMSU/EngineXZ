cd ~/Documents/GitHub/EngineXZ/shellex_macos
ARCH=arm64
source thumbex.sh
source xxspex.sh
ertbuild test.ertproj -N
mkdir _build/test/test.app/Contents/PlugIns
mkdir _build/test/test.app/Contents/Library
mkdir _build/test/test.app/Contents/Library/Spotlight
cp -r _build/macosx_${ARCH}_release/xxqlex.appex _build/test/test.app/Contents/PlugIns
cp -r _build/macosx_${ARCH}_release/xxspex.mdimporter _build/test/test.app/Contents/Library/Spotlight
open _build/test/test.app