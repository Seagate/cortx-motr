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

M0_SRC_DIR=`readlink -f $0`
M0_SRC_DIR=${M0_SRC_DIR%/*/*/*}

. $M0_SRC_DIR/spiel/st/m0t1fs_spiel_dix_common_inc.sh

spiel_dix_repair_quiesce_test()
{
	local fail_device1=2

#	$DIXINIT_TOOL_DESTROY >/dev/null
#	$DIXINIT_TOOL_CREATE  >/dev/null
	echo "*** m0dixinit is omitted. Mkfs creates meta indices now."

	verify || return $?
	#######################################################################
	#  Now starting SPIEL DIX repair/rebalance quiesce/continue testing   #
	#######################################################################

	echo "Set Failure device: $fail_device1"
	cas_disk_state_set "failed" $fail_device1 || return $?

	echo "Device $fail_device1 failed. Do dgmode read"
	verify || return $?

	cas_disk_state_get $fail_device1 || return $?

	echo "Start DIX repair"
	cas_disk_state_set "repair" $fail_device1 || return $?
	spiel_dix_repair_start
	sleep 2

	echo "Quiesce DIX repair"
	spiel_dix_repair_quiesce

	echo "wait for dix repair"
	spiel_wait_for_dix_repair || return $?

	verify || return $?

	echo "Continue start DIX repair"
	spiel_dix_repair_continue
	sleep 3

	echo "wait for the continued dix repair"
	spiel_wait_for_dix_repair || return $?

	cas_disk_state_set "repaired" $fail_device1 || return $?
	echo "DIX Repair done."
	verify || return $?

	cas_disk_state_get $fail_device1 || return $?

	cas_disk_state_set "rebalance" $fail_device1 || return $?
	echo "Starting DIX Re-balance.."
	spiel_dix_rebalance_start
	sleep 2

	echo "Quiesce DIX rebalance"
	spiel_dix_rebalance_quiesce

	echo "wait for dix rebalance"
	spiel_wait_for_dix_rebalance || return $?

	echo "Continue DIX rebalance"
	spiel_dix_rebalance_continue

	echo "wait for continued dix rebalance"
	spiel_wait_for_dix_rebalance || return $?

	cas_disk_state_set "online" $fail_device1 || return $?
	echo "DIX Rebalance done."

	verify || return $?

	cas_disk_state_get $fail_device1 || return $?

	#######################################################################
	#  End                                                                #
	#######################################################################

	return 0
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


	spiel_prepare

	if [[ $rc -eq 0 ]] && ! spiel_dix_repair_quiesce_test ; then
		echo "Failed: DIX repair failed.."
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
report_and_exit spiel-dix-repair-quiesce $?
