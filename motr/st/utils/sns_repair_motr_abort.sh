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


TOPDIR=`dirname $0`/../../../

. ${TOPDIR}/m0t1fs/linux_kernel/st/common.sh
. ${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_common_inc.sh
. ${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_client_inc.sh
. ${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_server_inc.sh
. ${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_sns_common_inc.sh
. ${TOPDIR}/motr/st/utils/sns_repair_common_inc.sh


# The following variables 'files', 'unit_size', 'file_size'
# and 'N', 'K', 'S', 'P' will override corresponding variables in
# m0t1fs_sns_common_inc.sh
file=(
	10000:10000
	10001:10001
	10002:10002
	10003:10003
	10004:10004
	10005:10005
	10006:10006
	10007:10007
	10008:10008
	10009:10009
	10010:10010
	10011:10011
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
	4096
	8192
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
	36
	18
)


S=3
N=3
K=3
P=15
src_count=15
export MOTR_CLIENT_ONLY=1

sns_repair_motr_test()
{
	local fail_device1=1
	local fail_device2=9
	local fail_device3=3

	echo "Starting SNS repair Motr Abort testing ..."

	prepare_datafiles_and_objects || return $?
	motr_read_verify 0          || return $?

	for ((i=0; i < ${#IOSEP[*]}; i++)) ; do
		ios_eps="$ios_eps -S ${lnet_nid}:${IOSEP[$i]}"
	done

	echo "Set Failure device: $fail_device1 $fail_device2"
	disk_state_set "failed" $fail_device1 $fail_device2 || return $?

	echo "Device $fail_device1 and $fail_device2 failed. Do dgmode read"
	motr_read_verify 0 || return $?

	disk_state_get $fail_device1 $fail_device2 || return $?

	########################################################################
	# Start SNS abort test                                                 #
	########################################################################

	echo "Start SNS repair"
	disk_state_set "repair" $fail_device1 $fail_device2 || return $?
	sns_repair || return $?
	sleep 3

	echo "Abort SNS repair"
	sns_repair_abort

	echo "wait for sns repair"
	wait_for_sns_repair_or_rebalance "repair" || return $?

	echo "query sns repair status"
	sns_repair_or_rebalance_status "repair" || return $?

	echo "SNS Repair aborted."
	motr_read_verify 0 || return $?

	echo "Query device state:$fail_device1 $fail_device2"
	disk_state_get $fail_device1 $fail_device2 || return $?

	echo "Set Failure device: $fail_device3"
	disk_state_set "failed" $fail_device3 || return $?
	disk_state_set "repair" $fail_device3 || return $?

	echo "Start SNS repair again ..."
	sns_repair || return $?

	echo "wait for sns repair"
	wait_for_sns_repair_or_rebalance "repair" || return $?

	echo "query sns repair status"
	sns_repair_or_rebalance_status "repair" || return $?

	disk_state_set "repaired" $fail_device1 $fail_device2 \
				  $fail_device3 || return $?
	echo "SNS Repair done."
	motr_read_verify 0 || return $?

	echo "Query device state: $fail_device1 $fail_device2"
	disk_state_get $fail_device1 $fail_device2 $fail_device3 || return $?

	return $?
}

main()
{
	local rc=0

	sandbox_init

	NODE_UUID=`uuidgen`
	local multiple_pools=0
	motr_service start $multiple_pools $stride $N $K $S $P || {
		echo "Failed to start Motr Service."
		return 1
	}

	if [[ $rc -eq 0 ]] && ! sns_repair_motr_test ; then
		echo "Failed: SNS repair failed.."
		rc=1
	fi

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
report_and_exit sns-repair-motr-abort $?
