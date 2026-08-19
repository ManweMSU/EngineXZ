ARCH=x64
XVC_ARCH=x64
if [[ "$1" == "debug" || "$2" == "debug" ]]; then
MODE=debug
else
MODE=release
fi
if [[ "$1" == "nogui" || "$2" == "nogui" ]]; then
NOGUI=1
fi
source xx_release/build_linux.sh