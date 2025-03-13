cd ~/Documents/GitHub/EngineXZ/shellex_macos
if [ "$#" == "1" ]; then
ARCH=$1
fi
./ertxpc/bin/ertbuild xxspex.ertproj -Na ${ARCH}
xv xxspex.xv -Ndr _build/macosx_${ARCH}_release/xxspex.app/Contents spotlight.xml
rm -r _build/macosx_${ARCH}_release/xxspex.appex
mv _build/macosx_${ARCH}_release/xxspex.app _build/macosx_${ARCH}_release/xxspex.appex
codesign --sign - --entitlements common.entitlements _build/macosx_${ARCH}_release/xxspex.appex