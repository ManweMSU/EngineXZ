ARCH=x64
XVC_ARCH=x64
if [[ "$1" == "nogui" ]]; then
NOGUI=1
fi
source xv_release/build_linux.sh