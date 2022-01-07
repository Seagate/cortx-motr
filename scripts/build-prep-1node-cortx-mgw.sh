#!/bin/sh

# Clone, build and prepare to start a single-node cluster with mgw.
#
# Usage: ./build-prep-1node-cortx-mgw.sh [-dev]
#
#   -dev  development mode, don't build and install rpms
#   -mgs  keep ceph mgs which procvides dashboard
#
set -e -o pipefail

dev_mode=
mgs="--without-dashboard"

[[ $1 == "-dev" ]] && dev_mode='yes'

[[ $1 == "-mgs"  || $2 == "-mgs" ]] && mgs=""

# CPUS 8, 16G RAM, 2 disks of minimum 50 G each

available_device=`lsblk --output NAME,FSTYPE -dsn | awk '$2 == "" {print $1}' | grep sd | head -n 1` 
mkfs.ext4 /dev/$available_device
mkdir -p ~/cortx
mount /dev/$available_device ~/cortx

cd ~/cortx

yum install wget -y
wget https://raw.githubusercontent.com/Seagate/cortx-motr/main/scripts/build-prep-1node.sh
sh +x ./build-prep-1node.sh $1



echo "=========================================\n"
echo "CORTX build and intallation is completed.\n"
echo "=========================================\n"


echo "=========================================\n"
echo "Start Ceph RGW SAL with MOTR build .\n"
echo "=========================================\n"


[[ -d ceph ]] || {
   git clone --recurse https://github.com/Seagate/cortx-rgw.git
   cd cortx-rgw && git checkout mgw-stable
   ./install-deps.sh
}
cd -

cd ~/rgw-cortx/cortx-rgw
cmake3 -GNinja -DWITH_PYTHON3=3.6 -DWITH_RADOSGW_MOTR=YES -B build
cd build && time ninja vstart-base && time ninja radosgw-admin && time ninja radosgw
 
MDS=0 RGW=1 ../src/vstart.sh -d -n $mgs
MDS=0 RGW=1 ../src/vstart.sh -d $mgs

echo "=========================================\n"
echo "Ceph RGW SAL with MOTR build is completed.\n"
echo "=========================================\n"

echo
echo 'update singlenode.yaml with devices in "path: /dev/loop0" format'
echo 'Don't use /dev/$available_device in cluster yaml'
echo 'Don't use the devices with filesystem on them'
echo 'Now you are ready to start the singlenode Motr cluster!'
echo 'To start, run: hctl bootstrap --mkfs singlenode.yaml'
echo 'To check:     hctl status'
echo 'To shutdown:  hctl shutdown'
echo "Follow the link for further steps"
echo "https://seagate-systems.atlassian.net/wiki/spaces/PRIVATECOR/pages/808583540/Motr+RGW+MGW+on+single-node+HOWTO"
echo "Update ceph.conf as mentioned in above link"
echo "Then MDS=0 RGW=1 ../src/vstart.sh -d"
