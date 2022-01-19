#!/bin/sh

# Clone, build and prepare to start a single-node cluster with mgw.
# It uses first avaialble disk of type /dev/sd*
#
# Usage: ./build-prep-1node-cortx-mgw.sh [-dev] [-pkg] [-mgs]
#
#   -dev  development mode, don't build and install rpms
#   -mgs  keep ceph mgs which provides dashboard
#   -pkg  install known packages without adding repo
#         (If this option is not specified motr/scripts/install-build-deps
#          is called which adds required repo and also build and installs
#          lustre, libfabric and isa packages and installs kernel packages too.)
#
set -e -o pipefail
#set -x

mgs="--without-dashboard"

[[ $1 == "-mgs"  || $2 == "-mgs" || $3 == "-mgs" ]] && mgs=""

# CPUS 8, 16G RAM, 2 disks of minimum 50 G each

[[ -d cortx ]] || {
  available_device=$(lsblk --output NAME,FSTYPE -dsn | awk '$2 == "" {print $1}' | grep sd | head -n 1)
  mkfs.ext4 "/dev/$available_device"
  mkdir -p ~/cortx
  mount "/dev/$available_device" ~/cortx
}

cd ~/cortx

yum install wget -y
wget https://raw.githubusercontent.com/Seagate/cortx-motr/main/scripts/build-prep-1node.sh
sh +x ~/build-prep-1node.sh "$1" "$2"
rm build-prep-1node.sh


echo "========================================="
echo "CORTX build and intallation is completed."
echo "========================================="


echo "========================================="
echo "Start Ceph RGW SAL with MOTR build ."
echo "========================================="


[[ -d ceph ]] || {
   git clone --recurse https://github.com/Seagate/cortx-rgw.git
   cd cortx-rgw
   sudo ./install-deps.sh
}
cd -

cd ~/cortx/cortx-rgw
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
echo 'Do not use "/dev/$available_device" in cluster yaml'
echo 'Also do not use the devices with filesystem on them'
echo 'Now you are ready to start the singlenode Motr cluster!'
echo 'To start, run: hctl bootstrap --mkfs singlenode.yaml'
echo 'To check:     hctl status'
echo 'To shutdown:  hctl shutdown'
echo "Follow the link for further steps"
echo "https://seagate-systems.atlassian.net/wiki/spaces/PRIVATECOR/pages/808583540/Motr+RGW+MGW+on+single-node+HOWTO"
echo 'run: hctl bootstrap --mkfs singlenode.yaml'
echo "Update ceph.conf as mentioned in above link"
echo "run:  MDS=0 RGW=1 ../src/vstart.sh -d $mgs"
echo "After reboot a VM run mount "/dev/$available_device" ~/cortx"
