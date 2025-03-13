cd ~/Documents/GitHub/EngineXZ/shellex_macos
if [ "$#" == "1" ]; then
ARCH=$1
fi
./ertxpc/bin/ertbuild xxspex.ertproj -Na ${ARCH}
rm -r _build/macosx_${ARCH}_release/xxspex.mdimporter
mkdir _build/macosx_${ARCH}_release/xxspex.mdimporter
mkdir _build/macosx_${ARCH}_release/xxspex.mdimporter/Contents
mkdir _build/macosx_${ARCH}_release/xxspex.mdimporter/Contents/MacOS
mkdir _build/macosx_${ARCH}_release/xxspex.mdimporter/Contents/Resources
mkdir _build/macosx_${ARCH}_release/xxspex.mdimporter/Contents/Resources/en.lproj
mkdir _build/macosx_${ARCH}_release/xxspex.mdimporter/Contents/Resources/la.lproj
mkdir _build/macosx_${ARCH}_release/xxspex.mdimporter/Contents/Resources/ru.lproj
cp _build/macosx_${ARCH}_release/xxspex.dylib _build/macosx_${ARCH}_release/xxspex.mdimporter/Contents/MacOS/xxspex.dylib
cp spotlight.plist _build/macosx_${ARCH}_release/xxspex.mdimporter/Contents/Info.plist
cp spotlight.xml _build/macosx_${ARCH}_release/xxspex.mdimporter/Contents/Resources/schema.xml
cp spotlight.en.strings _build/macosx_${ARCH}_release/xxspex.mdimporter/Contents/Resources/en.lproj/schema.strings
cp spotlight.la.strings _build/macosx_${ARCH}_release/xxspex.mdimporter/Contents/Resources/la.lproj/schema.strings
cp spotlight.ru.strings _build/macosx_${ARCH}_release/xxspex.mdimporter/Contents/Resources/ru.lproj/schema.strings
codesign --sign - --entitlements common.entitlements _build/macosx_${ARCH}_release/xxspex.mdimporter