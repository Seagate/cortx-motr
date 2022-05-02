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


#set -x

. `dirname $0`/common.sh
. `dirname $0`/m0t1fs_common_inc.sh
. `dirname $0`/m0t1fs_client_inc.sh
. `dirname $0`/m0t1fs_server_inc.sh
. `dirname $0`/m0t1fs_sns_common_inc.sh
. `dirname $0`/m0t1fs_io_config_params.sh

N=8
K=2
S=2
P=15
stride=32
fail_device1=1
fail_device2=9
fail_devices="$fail_device1 $fail_device2"
motr_module_path="$M0_SRC_DIR/motr"

spare_space_reserve_test()
{
	local rc=0
	local i=0
	BS=256M
	COUNT=4
	while [ $rc -eq 0 ]
	do
		local j=0:00$i
		m0t1fs_file=$MOTR_M0T1FS_MOUNT_DIR/${j}
		touch_file $m0t1fs_file 8192
		rc=$?
		if [ $rc -ne 0 ]
		then
			echo "rc=$rc"
			return $rc
		fi
		dd if=/dev/urandom of=$m0t1fs_file bs=$BS count=$COUNT
		rc=$?
		i=$(expr $i + 1)
	done
	df -h $MOTR_M0T1FS_MOUNT_DIR
	disk_state_set "failed" $fail_devices || {
		echo "Failed: disk_state_set failed for $device ..."
		return 1
	}
	disk_state_get $fail_devices
	disk_state_set "repair" $fail_devices || return 1
	sns_repair || {
		echo "Failed: SNS repair..."
		return 1
	}
	echo "wait for sns repair"
	wait_for_sns_repair_or_rebalance "repair" || return $?
	disk_state_set "repaired" $fail_devices || return 1
	sns_repair_or_rebalance_status "repair" || return $?
	echo "sns repair done"
	disk_state_get $fail_devices
	return 0
}

main()
{
	local rc=0

	sandbox_init

	NODE_UUID=`uuidgen`
	local multiple_pools=0
	local FI_OPTS="cs_storage_devs_init:spare_reserve:always"
	motr_service start $multiple_pools $stride $N $K $S $P $FI_OPTS || {
		echo "Failed to start Motr Service."
		return 1
	}
	ios_eps=""
	for ((i=0; i < ${#IOSEP[*]}; i++)) ; do
		ios_eps="$ios_eps -S ${lnet_nid}:${IOSEP[$i]}"
	done

	mount_m0t1fs $MOTR_M0T1FS_MOUNT_DIR "oostore" || {
		echo "mount failed"
		return 1
	}

	# Change this when spare-space reservation is enabled.
	if [[ $rc -eq 0 ]] && spare_space_reserve_test ; then
#	if [[ $rc -eq 0 ]] && ! spare_space_reserve_test ; then
		echo "Failed: spare-space-reservation test failed.."
		rc=1
	fi

	echo "unmounting and cleaning.."
	unmount_and_clean &>> $MOTR_TEST_LOGFILE

	motr_service stop || {
		echo "Failed to stop Motr Service."
		rc=1
	}
	if [ $rc -eq 0 ]; then
		sandbox_fini
	else
		echo "Test log available at $MOTR_TEST_LOGFILE."
	fi
	return $rc
}
trap unprepare EXIT
main
report_and_exit spare-space-reserve $?
