#! /bin/bash

# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Sanil Puthiyandyil
# Creation Date: 11/23/2014
# 

##
# This script is used to copy the riftware software into the qcow image
# This script must be run on the grunt machine as root
##

#set -x
#set -e

RIFTQCOW=/net/boson/home1/common/vm/rift-vnf-latest.qcow2
RIFTWORKAREA=$RIFT_ROOT


while [[ $# > 0 ]]
do
key=$1
case $key in
    -b|--base-image)
    RIFTQCOW="$2"
    shift # past argument
    ;;

    -w|--workarea)
    RIFTWORKAREA="$2"
    shift # past argument
    ;;

    -h|--help)
    echo "Usage: $0  -b|--base-image <input-qcow2-image>  -w|--workarea <rift-work-area>"
    exit 0
    ;;

    *)
            # unknown option
    echo "Usage: $0  -b|--base-image <input-qcow2-image>  -w|--workarea <rift-work-area>"
    exit 0
    ;;
esac
shift # past argument or value
done

if [ -z "$RIFTWORKAREA" ]; then
    echo "Usage: $0  -b|--base-image <input-qcow2-image>  -w|--workarea <rift-work-area>"
    exit 1
fi

echo "Using $RIFTQCOW as base image, $RIFTWORKAREA as work area to create RIFT VNF image"

CURRENT_DIR=$PWD

if [ -f $RIFTQCOW ]; then
  echo "file $RIFTQCOW found"
else
  echo "file $RIFTQCOW does not exist"
  exit 1
fi


if  [ -d $RIFTWORKAREA/.install ]; then
  echo "$RIFTWORKAREA/.install found!!"
else
  echo "$RIFTWORKAREA/.install not found!!"
  exit 1
fi

IMAGE_DIR="$RIFTWORKAREA"/.images

if  [ -d $IMAGE_DIR ]; then
  echo "$IMAGE_DIR directory exists - deleting..!!"
  if  [ -d $IMAGE_DIR/mnt ]; then
    guestunmount $IMAGE_DIR/mnt || true
  fi
  rm -rf $IMAGE_DIR
fi

mkdir $IMAGE_DIR
mkdir -p $IMAGE_DIR/mnt
RIFTVNFQCOW=rift-root-latest-multivm-vnf.qcow2

echo "Copying $RIFTQCOW"
cp $RIFTQCOW  $IMAGE_DIR/$RIFTVNFQCOW
chmod 755 $IMAGE_DIR/$RIFTVNFQCOW

echo "Mounting guestfs for $RIFTVNFQCOW"
guestmount -a $IMAGE_DIR/$RIFTVNFQCOW -m /dev/sda1 $IMAGE_DIR/mnt
trap "guestunmount $IMAGE_DIR/mnt" EXIT

echo "PEERDNS=yes" >> $IMAGE_DIR/mnt/etc/sysconfig/network-scripts/ifcfg-eth0
echo "DEFROUTE=yes" >> $IMAGE_DIR/mnt/etc/sysconfig/network-scripts/ifcfg-eth0
echo "#blacklist misc ethernet disable default starting - rift" >> $IMAGE_DIR/mnt/usr/lib/modprobe.d/dist-blacklist.conf
echo "blacklist ixgbevf" >> $IMAGE_DIR/mnt/usr/lib/modprobe.d/dist-blacklist.conf
echo "blacklist i40evf" >> $IMAGE_DIR/mnt/usr/lib/modprobe.d/dist-blacklist.conf
for i in `seq 1 4`
do
    cat <<EOF >> $IMAGE_DIR/mnt/etc/sysconfig/network-scripts/ifcfg-eth$i
DEVICE="eth$i"
BOOTPROTO="dhcp"
ONBOOT="yes"
TYPE="Ethernet"
DEFROUTE=no
PEERDNS=no
EOF
done

#Make .install smaller size - temporary work around, can do this by copying entire .install to a temp location - tbd
echo "reducing rift image size for image ..."

if  [ -d $RIFTWORKAREA/.install.temp ]; then
  rm -rf $RIFTWORKAREA/.install.temp
fi
cp -rL $RIFTWORKAREA/.install  $RIFTWORKAREA/.install.temp  || true
#cd $RIFTWORKAREA/.install.temp
rm -rf $RIFTWORKAREA/.install.temp/modules
rm -rf $RIFTWORKAREA/.install.temp/launchpad
rm -rf $RIFTWORKAREA/.install.temp/usr/rift/mano
rm -rf $RIFTWORKAREA/.install.temp/usr/include
rm -rf $RIFTWORKAREA/.install.temp/ltesim
rm -rf $RIFTWORKAREA/.install.temp/usr/share/gir-1.0
rm -rf $RIFTWORKAREA/.install.temp/usr/share/gir-1.0
rm -rf $RIFTWORKAREA/.install.temp/usr/share/rift/gir-1.0
#rm -rf $RIFTWORKAREA/.install.temp/usr/lib/ipsec
rm -rf $RIFTWORKAREA/.install.temp/usr/lib/libyangtools_test_yang_gi_gen.so
rm -rf $RIFTWORKAREA/.install.temp/usr/lib/libyangtools_test_yang_testing_gen.so
rm -rf $RIFTWORKAREA/.install.temp/usr/lib/libyangtools_test_yang_gtest_gen.so
rm -rf $RIFTWORKAREA/.install.temp/usr/lib/liblog_test_yang_gen.so
rm -rf $RIFTWORKAREA/.install.temp/usr/lib/libdts_test_yang_gen.so
rm -rf $RIFTWORKAREA/.install.temp/usr/share/rift/vapi/test*
rm -rf $RIFTWORKAREA/.install.temp/usr/lib/librwrestconf_test_yang_gen.so
#rm -rf $RIFTWORKAREA/.install.temp/usr/local/qat
rm -rf $RIFTWORKAREA/.install.temp/unique_ws*
rm -rf $RIFTWORKAREA/.install.temp/persist*
rm -rf $RIFTWORKAREA/.install.temp/var/rift/schema

du -sh $RIFTWORKAREA/.install.temp
#$CURRENT_DIR/links2files.sh
#cd $CURRENT_DIR

echo "Copying image ..."
mkdir $IMAGE_DIR/mnt/opt/rift
cp $RIFTWORKAREA/modules/core/enablement/vnf/image/content/scripts/start_multivmvnf $IMAGE_DIR/mnt/opt/rift
cp $RIFTWORKAREA/modules/core/enablement/vnf/image/content/scripts/multivmvnf.service $IMAGE_DIR/mnt/etc/systemd/system
cp $RIFTWORKAREA/modules/core/enablement/vnf/image/content/scripts/riftcli $IMAGE_DIR/mnt/usr/bin
cp /net/boson/home1/common/drivers/intel/i40evf-1.4.15/src/i40evf/i40evf.ko $IMAGE_DIR/mnt/opt/rift
chmod 755 $IMAGE_DIR/mnt/usr/bin/riftcli
#cp -ar /usr/lib/python2.7/site-packages/tornado $IMAGE_DIR/mnt/usr/lib/python2.7/site-packages/
cp -r $RIFTWORKAREA/rift-shell $IMAGE_DIR/mnt/home/rift
mkdir $IMAGE_DIR/mnt/home/rift/.build
mkdir $IMAGE_DIR/mnt/home/rift/.artifacts
cp  $RIFTWORKAREA/.build/.rift-env $IMAGE_DIR/mnt/home/rift/.build
cp  $RIFTWORKAREA/rift-bashrc $IMAGE_DIR/mnt/home/rift
cp  $RIFTWORKAREA/rift_env.py $IMAGE_DIR/mnt/home/rift
cp  $RIFTWORKAREA/rift-prompt $IMAGE_DIR/mnt/home/rift
cp -r $RIFTWORKAREA/scripts/* $IMAGE_DIR/mnt/home/rift/scripts
cp -r $RIFTWORKAREA/etc $IMAGE_DIR/mnt/home/rift
cp -r $RIFTWORKAREA/.install.temp $IMAGE_DIR/mnt/home/rift/.install

echo "Copying IPSec stuff to image ..."
mkdir -p $IMAGE_DIR/mnt/$RIFTWORKAREA/.install/usr
cp -R $RIFTWORKAREA/.install.temp/usr/etc $IMAGE_DIR/mnt/$RIFTWORKAREA/.install/usr

sync
sleep 30
guestunmount $IMAGE_DIR/mnt
sleep 10
virt-sparsify --compress $IMAGE_DIR/$RIFTVNFQCOW $IMAGE_DIR/"$RIFTVNFQCOW".comp 
sync
sleep 10
if [ $? -ne 0 ]; then
  echo "virt-sparsify failed - status $?"
  rm -rf $RIFTWORKAREA/.install.temp
  exit 1
fi
mv $IMAGE_DIR/"$RIFTVNFQCOW".comp $IMAGE_DIR/$RIFTVNFQCOW
sync
sleep 4
rm -rf $RIFTWORKAREA/.install.temp
