cd ~/Documents/GitHub/EngineXZ/shellex_macos
if [ "$#" == "1" ]; then
ARCH=$1
fi
./ertxpc/bin/ertbuild thumbex.ertproj -Na ${ARCH}
xv thumbex.xv -Ndr _build/macosx_${ARCH}_release/xxqlex.app/Contents ../artworks/xx_xx_icon.eiwv
rm -r _build/macosx_${ARCH}_release/xxqlex.appex
mv _build/macosx_${ARCH}_release/xxqlex.app _build/macosx_${ARCH}_release/xxqlex.appex
codesign --sign - --entitlements common.entitlements _build/macosx_${ARCH}_release/xxqlex.appex