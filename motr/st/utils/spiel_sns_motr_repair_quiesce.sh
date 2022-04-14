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
TOPDIR="$(dirname "$0")/../../../"

. "${TOPDIR}/spiel/st/m0t1fs_spiel_sns_common_inc.sh"
. "${TOPDIR}/motr/st/utils/sns_repair_common_inc.sh"
. "${TOPDIR}/m0t1fs/linux_kernel/st/common.sh"
. "${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_common_inc.sh"
. "${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_client_inc.sh"
. "${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_server_inc.sh"
. "${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_sns_common_inc.sh"
S=3
N=3
K=3
P=15
export MOTR_CLIENT_ONLY=1

spiel_sns_repair_quiesce_test()
{
	local fail_device2=9
	local fail_device3=3

	echo "Starting SNS repair testing ..."
	prepare_datafiles_and_objects || return "$?"
	motr_read_verify 0          || return "$?"

	for ((i=0; i < ${#IOSEP[*]}; i++)) ; do
		ios_eps="$ios_eps -S ${lnet_nid}:${IOSEP[$i]}"
	done

	#######################################################################
	#  Now starting SPIEL sns repair/rebalance quiesce/continue testing   #
	#######################################################################

	echo "Set Failure device: $fail_device2 $fail_device3"
	disk_state_set "failed" "$fail_device2" "$fail_device3" || return "$?"

	echo "Device $fail_device2 and $fail_device3 failed. Do dgmode read"
	motr_read_verify 0 || return "$?"

	disk_state_get "$fail_device2" "$fail_device3" || return "$?"

	echo "Start SNS repair"
	disk_state_set "repair" "$fail_device2" "$fail_device3" || return "$?"
	spiel_sns_repair_start
	sleep 2

	echo "Quiesce SNS repair"
	spiel_sns_repair_quiesce

	echo "wait for sns repair"
	spiel_wait_for_sns_repair || return "$?"

	motr_read_verify 0 || return "$?"

	echo "Continue start SNS repair"
	spiel_sns_repair_continue
	sleep 3

	echo "wait for the continued sns repair"
	spiel_wait_for_sns_repair || return "$?"

	disk_state_set "repaired" "$fail_device2" "$fail_device3" || return "$?"
	echo "SNS Repair done."
	motr_read_verify 0 || return "$?"

	disk_state_get "$fail_device2" "$fail_device3" || return "$?"

	disk_state_set "rebalance" "$fail_device2" "$fail_device3" || return "$?"
	echo "Starting SNS Re-balance.."
	spiel_sns_rebalance_start
	sleep 2

	echo "Quiesce SNS rebalance"
	spiel_sns_rebalance_quiesce

	echo "wait for sns rebalance"
	spiel_wait_for_sns_rebalance || return "$?"

	echo "Continue SNS rebalance"
	spiel_sns_rebalance_continue

	echo "wait for continued sns rebalance"
	spiel_wait_for_sns_rebalance || return "$?"

	disk_state_set "online" "$fail_device2" "$fail_device3" || return "$?"
	echo "SNS Rebalance done."

	motr_read_verify 0 || return "$?"

	disk_state_get "$fail_device2" "$fail_device3" || return "$?"

	#######################################################################
	#  End                                                                #
	#######################################################################

	return 0
}

main()
{
	local rc=0

	sandbox_init

	NODE_UUID=$(uuidgen)
	local multiple_pools=0
	motr_service start $multiple_pools $stride $N $K $S $P || {
		echo "Failed to start Motr Service."
		return 1
	}

	spiel_prepare

	if [[ $rc -eq 0 ]] && ! spiel_sns_repair_quiesce_test ; then
		echo "Failed: SNS repair failed.."
		rc=1
	fi

	spiel_cleanup

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
report_and_exit spiel-sns-repair-quiesce $?
