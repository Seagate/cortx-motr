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

###################################################
# SNS repair is only supported in COPYTOOL mode,
# because ios need to hash gfid to mds. In COPYTOOL
# mode, filename is the string format of gfid.
###################################################
files=(
	0:10000
	0:10001
	0:10002
	0:10003
	0:10004
	0:10005
	0:10006
	0:10007
	0:10008
	0:10009
	0:10010
	0:10011
)

unit_size=(
	4
	8
	16
	32
	64
	128
	256
	512
	1024
	2048
	2048
	2048
)

file_size=(
	500
	700
	300
	0
	400
	0
	600
	200
	100
	60
	60
	60
)


N=3
K=3
S=3
P=15
stride=32
src_bs=10M
src_count=2

testname="sns-repair-ios-fail"

verify()
{
	echo "verifying ..."
	for ((i=0; i < ${#files[*]}; i++)) ; do
		local_read $((${unit_size[$i]} * 1024)) ${file_size[$i]} || return $?
		read_and_verify ${files[$i]} $((${unit_size[$i]} * 1024)) ${file_size[$i]} || return $?
	done

	echo "file verification sucess"
}

sns_repair_test()
{
	local fail_device1=1
	local fail_device2=9
	local fail_device3=3

	local_write $src_bs $src_count || return $?

	echo "Starting SNS repair testing ..."
	for ((i=0; i < ${#files[*]}; i++)) ; do
		touch_file $MOTR_M0T1FS_MOUNT_DIR/${files[$i]} ${unit_size[$i]}
		_dd ${files[$i]} $((${unit_size[$i]} * 1024)) ${file_size[$i]}
	done

	verify || return $?

	for ((i=0; i < ${#IOSEP[*]}; i++)) ; do
		ios_eps="$ios_eps -S ${lnet_nid}:${IOSEP[$i]}"
	done

	mount

	########################################################################
	# Start SNS repair/abort test while ios fails                          #
	########################################################################

	echo "Set Failure device: $fail_device1 $fail_device2"
	disk_state_set "failed" $fail_device1 $fail_device2 || return $?

	echo "Device $fail_device1 and $fail_device2 failed. Do dgmode read"
	verify || return $?

	disk_state_get $fail_device1 $fail_device2 || return $?

	echo "Start SNS repair"
	disk_state_set "repair" $fail_device1 $fail_device2 || return $?
	sns_repair || return $?
	sleep 3

	echo "killing ios4 ..."
	kill_ios4_ioservice

	ha_notify_ios4_failure_or_online "failed"
	sleep 2

	echo "ios4 failed, we have to quiesce SNS repair"
	sns_repair_quiesce
	sleep 2

	wait_for_sns_repair_or_rebalance_not_4 "repair" || return $?

	echo "query sns repair status"
	sns_repair_or_rebalance_status_not_4 "repair" || return $?

	# XXX disk_state_get() sends an ha_msg to IOS4 which is killed.
	# disk_state_get $fail_device1 $fail_device2 || return $?

	echo "================================================================="
	echo "start over the failed ios4"
	start_ios4_ioservice
	sleep 5

	echo "HA notifies that ios4 online."
	ha_notify_ios4_failure_or_online "online"

	# Currently STs use dummy HA which do not send approriate failure vector.
	# So inorder to allocate spare after ios restart, we need to transition
	# to failed state, but after restart device is in repairing state. As we
	# cannot directly transition a device from repairing to failed state, we
	# go through chain of device state transitions.
	disk_state_set "repaired" $fail_device1 $fail_device2 || return $?
	disk_state_set "rebalance" $fail_device1 $fail_device2 || return $?
	disk_state_set "online" $fail_device1 $fail_device2 || return $?
	disk_state_set "failed" $fail_device1 $fail_device2 || return $?
	disk_state_set "repair" $fail_device1 $fail_device2 || return $?

	echo "Start SNS repair again ..."
	sns_repair_resume || return $?

	echo "wait for sns repair"
	wait_for_sns_repair_or_rebalance "repair" || return $?

	echo "query sns repair status"
	sns_repair_or_rebalance_status "repair" || return $?

	disk_state_set "repaired" $fail_device1 $fail_device2 || return $?
	echo "SNS Repair done."
	verify || return $?

	disk_state_get $fail_device1 $fail_device2 || return $?

	echo "Start SNS rebalance"
	disk_state_set "rebalance" $fail_device1 $fail_device2 || return $?
	sns_rebalance || return $?
	sleep 3

	echo "killing ios4 ..."
	kill_ios4_ioservice

	ha_notify_ios4_failure_or_online "failed"
	sleep 2

	echo "quiescing rebalance"
	sns_rebalance_quiesce
	sleep 2

	wait_for_sns_repair_or_rebalance_not_4 "rebalance" || return $?

	echo "query sns repair status"
	sns_repair_or_rebalance_status_not_4 "rebalance" || return $?

	echo "================================================================="
	echo "start over the failed ios4"
	start_ios4_ioservice
	sleep 5

	echo "HA notifies that ios4 online."
	ha_notify_ios4_failure_or_online "online"

	disk_state_set "online" $fail_device1 $fail_device2 || return $?
	disk_state_set "failed" $fail_device1 $fail_device2 || return $?
	disk_state_set "repair" $fail_device1 $fail_device2 || return $?
	disk_state_set "repaired" $fail_device1 $fail_device2 || return $?
	disk_state_set "rebalance" $fail_device1 $fail_device2 || return $?

	echo "Start SNS rebalance again ..."
	sns_rebalance_resume || return $?

	echo "wait for sns rebalance"
	wait_for_sns_repair_or_rebalance "rebalance" || return $?

	echo "query sns repair status"
	sns_repair_or_rebalance_status "rebalance" || return $?

	disk_state_set "online" $fail_device1 $fail_device2 || return $?
	echo "SNS Rebalance done."
	verify || return $?

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
	return $rc
}

trap unprepare EXIT
main
report_and_exit $testname $?
