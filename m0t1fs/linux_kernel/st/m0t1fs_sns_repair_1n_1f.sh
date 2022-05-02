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


. `dirname $0`/common.sh
. `dirname $0`/m0t1fs_common_inc.sh
. `dirname $0`/m0t1fs_client_inc.sh
. `dirname $0`/m0t1fs_server_inc.sh
. `dirname $0`/m0t1fs_sns_common_inc.sh

###################################################
# SNS repair is only supported in COPYTOOL mode,
# because ios need to hash gfid to mds. In COPYTOOL
# mode, filename is the string format of gfid.
###################################################

sns_repair_test()
{
	local rc=0
	local fail_device1=1
	local unit_size=$((stride * 1024))

	echo "Starting SNS repair testing ..."

	local_write $src_bs $src_count || return $?

	for ((i=0; i < ${#file[*]}; i++)) ; do
		_dd ${file[$i]} $unit_size ${file_size[$i]} || return $?
		_md5sum ${file[$i]} || return $?
	done

	ios_eps="$ios_eps -S ${lnet_nid}:${IOSEP[0]}"

####### Set Failure device
	disk_state_set "failed" $fail_device1 || return $?

	disk_state_get $fail_device1 || return $?

	echo "Device $fail_device1 failed. Do dgmode read"
	md5sum_check || return $?

	disk_state_set "repair" $fail_device1 || return $?
	sns_repair || return $?

####### Abort repair and start again
	echo "Abort SNS repair"
	sns_repair_abort

	echo "wait for sns repair"
	wait_for_sns_repair_or_rebalance "repair" || return $?

	echo "query sns repair status"
	sns_repair_or_rebalance_status "repair" || return $?

	echo "Repair $fail_device1"
	sns_repair || return $?

	echo "wait for sns repair"
	wait_for_sns_repair_or_rebalance "repair" || return $?

	disk_state_set "repaired" $fail_device1 || return $?

	echo "query sns repair status"
	sns_repair_or_rebalance_status "repair" || return $?

	echo "SNS Repair done."
	md5sum_check || return $?

	echo "Query device state"
	disk_state_get $fail_device1 || return $?

	disk_state_set "rebalance" $fail_device1 || return $?
	echo "Starting SNS Re-balance.."
	sns_rebalance || return $?

	disk_state_get $fail_device1

	echo "wait for sns rebalance"
	wait_for_sns_repair_or_rebalance "rebalance" || return $?

	disk_state_set "online" $fail_device1 || return $?

	echo "query sns repair status"
	sns_repair_or_rebalance_status "rebalance" || return $?

	disk_state_get $fail_device1

	echo "SNS Re-balance done."

	echo "Verifying checksums.."
	md5sum_check || return $?

	return $?
}

main()
{
	local rc=0

	sandbox_init

	NODE_UUID=`uuidgen`
	local multiple_pools=0

	SINGLE_NODE=1

	motr_service start $multiple_pools $stride $N $K $S $P || {
		echo "Failed to start Motr Service."
		SINGLE_NODE=0
		return 1
	}

	sns_repair_mount || rc=$?

	if [[ $rc -eq 0 ]] && ! sns_repair_test ; then
		echo "Failed: SNS repair failed.."
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

	SINGLE_NODE=0

	return $rc
}

trap unprepare EXIT
main
report_and_exit sns-single-1n-1f $?
