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

SANDBOX_DIR=${SANDBOX_DIR:-/var/motr/sandbox.m0t1fs-writesize-st}
M0_SRC_DIR=`realpath $0`
M0_SRC_DIR=${M0_SRC_DIR%/*/*}
cd "$M0_SRC_DIR"

echo "Installing Motr services"
sudo scripts/install-motr-service -u
sudo scripts/install-motr-service -l
sudo utils/m0setup -v -P 12 -N 2 -K 1 -S 1 -i 3 -d /var/motr/img -s 1 -c
sudo utils/m0setup -v -P 12 -N 2 -K 1 -S 1 -i 3 -d /var/motr/img -s 1
echo "Start Motr services"
sudo systemctl start motr-mkfs
sudo systemctl start motr-singlenode

touch /mnt/m0t1fs/12345:1
stat /mnt/m0t1fs/12345:1
oldblksize=`stat /mnt/m0t1fs/12345:1 | grep "IO Block" | sed -e 's/.*IO Block:[[:space:]]//' -e 's/[[:space:]]reg.*//'`

dd if=/dev/zero of=/mnt/m0t1fs/12345:1 bs=1048576 count=10
stat /mnt/m0t1fs/12345:1
blksize=`stat /mnt/m0t1fs/12345:1 | grep "IO Block" | sed -e 's/.*IO Block:[[:space:]]//' -e 's/[[:space:]]reg.*//'`

if test "$blksize" -eq 1048576 -a "$oldblksize" -ne 1048576; then
	echo "Successfully set IO Block on first write"
else
	echo "IO Block size is not set correctly"
	sudo systemctl stop motr-singlenode
	exit 1
fi

touch /mnt/m0t1fs/12345:2
stat /mnt/m0t1fs/12345:2
oldblksize=`stat /mnt/m0t1fs/12345:2 | grep "IO Block" | sed -e 's/.*IO Block:[[:space:]]//' -e 's/[[:space:]]reg.*//'`

setfattr -n writesize -v "1048576" /mnt/m0t1fs/12345:2
stat /mnt/m0t1fs/12345:2
blksize=`stat /mnt/m0t1fs/12345:2 | grep "IO Block" | sed -e 's/.*IO Block:[[:space:]]//' -e 's/[[:space:]]reg.*//'`

if test "$blksize" -eq 1048576 -a "$oldblksize" -ne 1048576; then
	echo "Successfully set IO Block on setfattr"
else
	echo "IO Block size is not set correctly"
	sudo systemctl stop motr-singlenode
	exit 1
fi

echo "Tear down Motr services"
sudo systemctl stop motr-singlenode
sudo scripts/install-motr-service -u

# this msg is used by Jenkins as a test success criteria;
# it should appear on STDOUT
if [ $? -eq 0 ] ; then
    echo "m0d-m0t1fs-writesize: test status: SUCCESS"
fi
