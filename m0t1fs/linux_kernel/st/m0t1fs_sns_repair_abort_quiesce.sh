#!/usr/bin/env bash
#
# Copyright (c) 2020-2021 Seagate Technology LLC and/or its Affiliates
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

file=(
	0:10000
	0:10001
	0:10002
)

file_size=(
	200
	100
	400
)

N=3
K=3
S=3
P=15
stride=32
src_bs=10M
src_count=20

unit_size=$((stride * 1024))

testname="sns-repair-abort-repair-quiesce-rebalance-quiesce"

verify()
{
	echo "verifying ..."
	for ((i=0; i < ${#file[*]}; i++)) ; do
		local_read $unit_size ${file_size[$i]} || return $?
		read_and_verify ${file[$i]} $unit_size ${file_size[$i]} || return $?
	done

	echo "file verification sucess"
}

sns_repair_rebalance_quiesce_test()
{
	local rc=0
	local fail_device1=1
	local fail_device2=9
	local unit_size=$((stride * 1024))

	echo "Starting SNS repair/rebalance quiesce testing ..."

	local_write $src_bs $src_count || return $?

	for ((i=0; i < ${#file[*]}; i++)) ; do
		_dd ${file[$i]} $unit_size ${file_size[$i]} || return $?
		_md5sum ${file[$i]} || return $?
	done

	for ((i=0; i < ${#IOSEP[*]}; i++)) ; do
		ios_eps="$ios_eps -S ${lnet_nid}:${IOSEP[$i]}"
	done

####### Set Failure device
	disk_state_set "failed" $fail_device1 $fail_device2 || return $?

	echo "Device $fail_device1 $fail_device2 failed. Do dgmode read"
	md5sum_check || return $?

	echo "Starting SNS repair, and this will be aborted"
	disk_state_set "repair" $fail_device1 $fail_device2 || return $?
	sns_repair || return $?
	sleep 5

	# sending out ABORT cmd
	sns_repair_abort

	echo "wait for sns repair"
	wait_for_sns_repair_or_rebalance "repair" || return $?

        echo "SNS Repair aborted."
        verify || return $?

	echo "Query device state"
	disk_state_get $fail_device1 $fail_device2 || return $?

	echo "Continue SNS repair...this will be quiesced"
	sns_repair || return $?
	sleep 3

	echo "Sending QUIESCE cmd"
	sns_repair_quiesce

	echo "wait for sns repair"
	wait_for_sns_repair_or_rebalance "repair" || return $?

	echo "Continue SNS repair..."
	sns_repair || return $?
	sleep 3

	echo "wait for sns repair"
	wait_for_sns_repair_or_rebalance "repair" || return $?

	echo "SNS Repair done."
	disk_state_set "repaired" $fail_device1 $fail_device2 || return $?
	md5sum_check || return $?

	echo "Query device state"
	disk_state_get $fail_device1 $fail_device2 || return $?

	echo "Starting SNS Re-balance, and this will be aborted"
	disk_state_set "rebalance" $fail_device1 $fail_device2 || return $?
	sns_rebalance
	sleep 3

	echo "Aborting SNS rebalance"
	sns_rebalance_abort
	wait_for_sns_repair_or_rebalance "rebalance" || return $?

	echo "Continue SNS Re-balance..."
	sns_rebalance || return $?

	echo "Aborting SNS rebalance after quiesce"
	sns_rebalance_quiesce

	wait_for_sns_repair_or_rebalance "rebalance" || return $?

	echo "Continue SNS Re-balance..."
	sns_rebalance || return $?

	echo "wait for sns rebalance"
	wait_for_sns_repair_or_rebalance "rebalance" || return $?

	disk_state_set "online" $fail_device1 $fail_device2 || return $?
	echo "SNS Re-balance done."

	# Query device state
	disk_state_get $fail_device1 $fail_device2 || return $?

	echo "Verifying checksums.."
	md5sum_check || return $?

	disk_state_get $fail_device1 $fail_device2

	return $?
}

main()
{
	local rc=0

	check_test_skip_list $testname || return $rc

	sandbox_init

	NODE_UUID=`uuidgen`
	local multiple_pools=0
	motr_service start $multiple_pools $stride $N $K $S $P || {
		echo "Failed to start Motr Service."
		return 1
	}

	sns_repair_mount || rc=$?

	if [[ $rc -eq 0 ]] && ! sns_repair_rebalance_quiesce_test ; then
		echo "Failed: SNS repair/rebalance quiesce failed.."
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
report_and_exit $testname $?
