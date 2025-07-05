cd ~/Documents/GitHub/EngineXZ
./xv_release/build_macos_x64.sh
./xv_release/build_macos_arm64.sh
./xx_release/build_macos_x64.sh
./xx_release/build_macos_arm64.sh
xv com_release/fid.xv -Ndr 'xv_release/_build/macosx_x64/XV Monstrans Manualis.app/Contents/MacOS/xi'\
    'xx_release/_build/macosx_x64/XX.app/xxcl'\
    'xx_release/_build/macosx_x64/XX.app/xxi'\
    'xx_release/_build/macosx_arm64/XX.app/xxcl'\
    'xx_release/_build/macosx_arm64/XX.app/xxi'
xv com_release/arc.xv -Ndr 'xv_release/_build/macosx_x64/XV Monstrans Manualis.app'\
    com_release/manifesta/xv_mac.manifest com_release/xv_10_b7_mac_x64.ecsa
xv com_release/arc.xv -Ndr 'xv_release/_build/macosx_arm64/XV Monstrans Manualis.app'\
    com_release/manifesta/xv_mac.manifest com_release/xv_10_b7_mac_arm64.ecsa
xv com_release/arc.xv -Ndr 'xx_release/_build/macosx_x64/XX.app'\
    com_release/manifesta/xx_mac.manifest com_release/xx_10_b7_mac_x64.ecsa
xv com_release/arc.xv -Ndr 'xx_release/_build/macosx_arm64/XX.app'\
    com_release/manifesta/xx_mac.manifest com_release/xx_10_b7_mac_arm64.ecsa
./purify.sh