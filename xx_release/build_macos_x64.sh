cd /Users/manwe/Documents/GitHub/EngineJIT
ARCH=x64
XVC_ARCH=x64
if [ "$1" == "debug" ]; then
MODE=debug
else
MODE=release
fi
source xx_release/build_macos.sh