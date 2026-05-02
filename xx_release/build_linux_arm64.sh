ARCH=arm64
XVC_ARCH=arm64
if [ "$1" == "debug" ]; then
MODE=debug
else
MODE=release
fi
source xx_release/build_linux.sh