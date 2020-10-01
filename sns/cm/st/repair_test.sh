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

# Script to perform system tests for single node sns repair.
# Uses various combination of values for N, K, P and unit size parameters of
# pdclust layout used by sns repair.
# Working:
# 1) Start motr service using m0mount
#    - This also mount m0t1fs
# 2) Write data to mounted m0t1fs
# 3) Once i/o is done, start repair.
# Note: This script should be run from motr directory

M0_SRC_DIR=`realpath $0`
M0_SRC_DIR=${M0_SRC_DIR%/*/*/*/*}

N=(8 10 11 12 13 14 15 16 18 20)

K=1

P=(10 15 20 25 30 40 50 60 70 80)

U=(262144 524288 1024000 2048000 4096000 8192000 10485670 10485760 10485760 10485760)

cleanup()
{
	umount /mnt/m0

	pkill -INT -f m0d

	while ps ax | grep -v grep | grep m0d; do
		sleep 2;
	done

	rmmod m0tr
}

main()
{
	for ((i = 0; i < ${#P[*]}; i++)); do
		cmd="$M0_SRC_DIR/utils/m0mount -a -L -n 1 -d ${N[$i]} -p ${P[$i]} -u ${U[$i]} -vv -q"
		if ! $cmd
		then
			echo "Cannot start motr service"
			return 1
		fi

		cmd="dd if=/dev/zero of=/mnt/m0/dat bs=20M count=5000"
		if ! $cmd
		then
			echo "write failed"
			cleanup
			return 1
		fi

		cmd="$M0_SRC_DIR/sns/cm/st/m0repair -O 2 -t 0 -U ${U[$i]}
-F ${N[$i]} -n 1 -s 100000000000 -N ${N[$i]} -K 1 -P ${P[$i]}
-C 172.18.50.45@o2ib:12345:41:102 -S 172.18.50.45@o2ib:12345:41:101"
		if ! $cmd
		then
			echo "SNS Repair failed"
			cleanup
			return 1
		else
			echo "SNS Repair done."
		fi

		cleanup
	done

	return 0
}

#trap unprepare EXIT

#set -x

main

# Local variables:
# sh-basic-offset: 8
# sh-indentation: 8
# tab-width: 8
# End:
