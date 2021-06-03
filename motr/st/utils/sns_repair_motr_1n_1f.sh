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


export MOTR_CLIENT_ONLY=1

sns_repair_motr_test()
{
	local rc=0
	local fail_device1=1

	echo "Starting SNS repair MOTR 1n-1f testing ..."

	prepare_datafiles_and_objects || return $?
	motr_read_verify 0          || return $?

	ios_eps="$ios_eps -S ${lnet_nid}:${IOSEP[0]}"

####### Set Failure device
	disk_state_set "failed" $fail_device1 || return $?

	disk_state_get $fail_device1 || return $?

	echo "Device $fail_device1 failed. Do dgmode read"
	motr_read_verify 0 || return $?

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
	motr_read_verify 0 || return $?

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

	echo "Verifying objects..."
	motr_read_verify 0 || return $?

	return $?
}

main()
{
	local rc=0

	sandbox_init

	NODE_UUID=`uuidgen`
	local multiple_pools=0

	SINGLE_NODE=1

	motr_service start $multiple_pools $stride $N $K $P || {
		echo "Failed to start Motr Service."
		SINGLE_NODE=0
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

	SINGLE_NODE=0

	return $rc
}

trap unprepare EXIT
main
report_and_exit sns-repair-motr-1n-1f $?
