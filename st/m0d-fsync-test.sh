#!/usr/bin/env bash
#
# Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For any questions about this software or licensing,
# please email opensource@seagate.com or cortx-questions@seagate.com.
#

set -e

[[ $UID -eq 0 ]] || {
    echo 'Please, run this script with "root" privileges.' >&2
    exit 1
}


SANDBOX_DIR=${SANDBOX_DIR:-/var/motr/sandbox.fsync-st}
M0_SRC_DIR=`readlink -f $0`
M0_SRC_DIR=${M0_SRC_DIR%/*/*}

. $M0_SRC_DIR/utils/functions # report_and_exit

cd $M0_SRC_DIR

echo "Installing Motr services"
scripts/install-motr-service -u
rm -rf /etc/motr
rm -f  "$SYSCONFIG_DIR/motr"
scripts/install-motr-service -l
utils/m0setup -v -P 12 -N 2 -K 1 -i 3 -d /var/motr/img -s 128 -c
utils/m0setup -v -P 12 -N 2 -K 1 -i 3 -d /var/motr/img -s 128

# update Motr configuration: turn on fdatasync with '-I' option
sed -i "s/.*MOTR_M0D_EXTRA_OPTS.*/MOTR_M0D_EXTRA_OPTS='-I'/" \
     "$SYSCONFIG_DIR/motr"

# update Motr configuration: set specific dir for test artifacts
sed -i "s@.*MOTR_LOG_DIR.*@MOTR_LOG_DIR=${SANDBOX_DIR}/log@" \
     "$SYSCONFIG_DIR/motr"
sed -i "s@.*MOTR_M0D_DATA_DIR.*@MOTR_M0D_DATA_DIR=${SANDBOX_DIR}/motr@" \
     "$SYSCONFIG_DIR/motr"

echo "Start Motr services"
systemctl start motr-mkfs
systemctl start motr-singlenode

sleep 10 # allow motr to finish its startup

echo "Perform fsync test"
for i in 0:1{0..9}0000; do touch /mnt/m0t1fs/$i & done
for i in $(jobs -p) ; do wait $i ; done

for i in 0:1{0..9}0000; do setfattr -n lid -v 8 /mnt/m0t1fs/$i & done
for i in $(jobs -p) ; do wait $i ; done

for i in 0:1{0..9}0000; do dd if=/dev/zero of=/mnt/m0t1fs/$i \
    bs=8M count=20 conv=fsync & done
for i in $(jobs -p) ; do wait $i ; done

echo "Tear down Motr services"
systemctl stop motr-singlenode
systemctl stop motr-kernel
rc=$?
utils/m0setup -v -P 12 -N 2 -K 1 -i 3 -d /var/motr/img -s 8 -c
scripts/install-motr-service -u

if [ $rc -eq 0 ]; then
    rm -r $SANDBOX_DIR
fi

report_and_exit m0d-fsync $rc
