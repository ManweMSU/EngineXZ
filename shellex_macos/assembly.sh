cd ~/Documents/GitHub/EngineXZ/shellex_macos
ertbuild xx-qlex.ertproj -Nar x64
ertbuild xx-qlex.ertproj -Nar arm64
mkdir _build/plugin
mkdir _build/plugin/XX-x64.qlgenerator
mkdir _build/plugin/XX-arm64.qlgenerator
mkdir _build/plugin/XX-x64.qlgenerator/Contents
mkdir _build/plugin/XX-arm64.qlgenerator/Contents
mkdir _build/plugin/XX-x64.qlgenerator/Contents/MacOS
mkdir _build/plugin/XX-arm64.qlgenerator/Contents/MacOS
mkdir _build/plugin/XX-x64.qlgenerator/Contents/Resources
mkdir _build/plugin/XX-arm64.qlgenerator/Contents/Resources
eimgconv ../artworks/xx_xx_icon.eiwv -Nfo icns _build/plugin/XX-x64.qlgenerator/Contents/Resources/Icon.icns
eimgconv ../artworks/xx_xx_icon.eiwv -Nfo icns _build/plugin/XX-arm64.qlgenerator/Contents/Resources/Icon.icns
cp Info.plist _build/plugin/XX-x64.qlgenerator/Contents/Info.plist
cp Info.plist _build/plugin/XX-arm64.qlgenerator/Contents/Info.plist
cp _build/macosx_x64_release/xx-qlex.dylib _build/plugin/XX-x64.qlgenerator/Contents/MacOS/xx-qlex.dylib
cp _build/macosx_arm64_release/xx-qlex.dylib _build/plugin/XX-arm64.qlgenerator/Contents/MacOS/xx-qlex.dylib