#!/bin/sh

# Clone, build and prepare to start a single-node cluster with mgw.
# It uses first available disk of type /dev/sd*. Alternatively if
# required space is available in a directory, go to that directory
# and create a directory named "cortx" and run this script.
#
# Usage: ./build-prep-1node-cortx-mgw.sh [-dev] [-pkg] [-mgs]
#
#   -dev  development mode, don't build and install rpms
#   -mgs  keep ceph mgs which provides dashboard
#   -pkg  quickly install required packages without building them
#         (If this option is not specified motr/scripts/install-build-deps
#          is called which adds required repo and also build and installs
#          lustre, libfabric and isa packages and installs kernel packages too.)
#
set -e -o pipefail
#set -x

CORTX_WDIR=$PWD/cortx

mgs="--without-dashboard"

[[ $1 == "-mgs"  || $2 == "-mgs" || $3 == "-mgs" ]] && mgs=""

# CPUS 8, 16G RAM, 2 disks of minimum 50 G each

[[ -d cortx ]] || {
  available_device=$(lsblk --output NAME,FSTYPE -dsn | awk '$2 == "" {print $1}' | grep sd | head -n 1)
  mkfs.ext4 "/dev/$available_device"
  mkdir -p $CORTX_WDIR
  mount "/dev/$available_device" $CORTX_WDIR
}

cd $CORTX_WDIR

yum install wget -y
wget https://raw.githubusercontent.com/Seagate/cortx-motr/main/scripts/build-prep-1node.sh
sh +x ./build-prep-1node.sh "$1" "$2"
rm build-prep-1node.sh


echo "========================================="
echo "CORTX build and installation is completed."
echo "========================================="


echo "========================================="
echo "Start Ceph RGW SAL with MOTR build ."
echo "========================================="


[[ -d ceph ]] || {
   git clone --recurse https://github.com/Seagate/cortx-rgw.git
   cd cortx-rgw
   sudo ./install-deps.sh
}

cd $CORTX_WDIR/cortx-rgw
cmake3 -GNinja -DWITH_PYTHON3=3.6 -DWITH_RADOSGW_MOTR=YES -B build
cd build && time ninja vstart-base && time ninja radosgw-admin && time ninja radosgw

echo "Start Ceph RGW.." 
MDS=0 RGW=1 ../src/vstart.sh -d -n "$mgs"
../src/stop.sh


echo "========================================="
echo "Ceph RGW SAL with MOTR build is completed."
echo "========================================="

echo
echo 'update singlenode.yaml with devices in "path: /dev/loop0" format'
echo "Do not use /dev/$available_device in cluster yaml"
echo 'Also do not use the devices with filesystem on them'
echo 'Now you are ready to start the singlenode Motr cluster!'
echo "To start, run: hctl bootstrap --mkfs  $CORTX_WDIR/singlenode.yaml"
echo 'check:     hctl status'
echo 'To connect to Motr cluster, add the following configuration'
echo "parameters from 'hctl status' to $CORTX_WDIR/cortx-rgw/build/ceph.conf:"
echo '[client]'
echo '      ...'
echo '      rgw backend store = motr'
echo '      motr profile fid  = 0x7000000000000001:0x4f'
echo '      motr ha endpoint  = inet:tcp:10.0.0.1@2001'
echo '      ...'
echo '[client.rgw.8000]'
echo '      ...'
echo '      motr my endpoint  = inet:tcp:10.0.0.1@5001'
echo '      motr my fid       = 0x7200000000000001:0x29'
echo "Current directory: $PWD"
echo "Then run:  MDS=0 RGW=1 ../src/vstart.sh -d $mgs"
echo 'check:     hctl status'
echo 'm0client should be in a started state'
echo 'To stop the cluster run: ../src/stop.sh'
echo 'Then:  hctl shutdown'
echo 'If local directory is not used then after reboot of the node,'
echo "run: mount /dev/$available_device $CORTX_WDIR"
