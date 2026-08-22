mkdir /tmp/resurrectio-systemae-xx || { echo "Error creandi collectorii primi."; rm -rf /tmp/resurrectio-systemae-xx; exit 1; }
cd /tmp/resurrectio-systemae-xx
git clone https://github.com/ManweMSU/ESSE || { echo "Error onerandi repositorii ESSE."; rm -rf /tmp/resurrectio-systemae-xx; exit 1; }
git clone https://github.com/ManweMSU/EngineXZ || { echo "Error onerandi repositorii X."; rm -rf /tmp/resurrectio-systemae-xx; exit 1; }
cd /tmp/resurrectio-systemae-xx/ESSE
./resurrect/resurrect-esse.sh || { rm -rf /tmp/resurrectio-systemae-xx; exit 1; }
export PATH=$PATH:/tmp/resurrectio-systemae-xx/ESSE/bin
cd /tmp/resurrectio-systemae-xx/EngineXZ
esse shellex_linux/xx-lxi.esse -N || { echo "Error struendi installatoris #1."; rm -rf /tmp/resurrectio-systemae-xx; exit 1; }
XLXI=`esse shellex_linux/xx-lxi.esse -OSE` || { echo "Error struendi installatoris #2."; rm -rf /tmp/resurrectio-systemae-xx; exit 1; }
ARCH=`$XLXI --verbum-modi` || { echo "Error struendi installatoris #3."; rm -rf /tmp/resurrectio-systemae-xx; exit 1; }
./xv_release/build_linux_${ARCH}.sh nogui
export PATH=/tmp/resurrectio-systemae-xx/EngineXZ/xv_release/_build/linux_${ARCH}:$PATH
$XLXI --crea-installationem-cui . /tmp/resurrectio-systemae-xx/xx-linux.ecsa || { echo "Installatio cancellata est #1."; rm -rf /tmp/resurrectio-systemae-xx; exit 1; }
$XLXI --installa /tmp/resurrectio-systemae-xx/xx-linux.ecsa || { echo "Installatio cancellata est #2."; rm -rf /tmp/resurrectio-systemae-xx; exit 1; }
rm -rf /tmp/resurrectio-systemae-xx