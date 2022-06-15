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

M0_SRC_DIR="$(readlink -f "${BASH_SOURCE[0]}")"
M0_SRC_DIR="${M0_SRC_DIR%/*/*/*/*}"

. "$M0_SRC_DIR"/m0t1fs/linux_kernel/st/m0t1fs_common_inc.sh
. "$M0_SRC_DIR"/m0t1fs/linux_kernel/st/m0t1fs_server_inc.sh
. "$M0_SRC_DIR"/m0t1fs/linux_kernel/st/m0t1fs_dix_common_inc.sh
. "$M0_SRC_DIR"/spiel/st/m0t1fs_spiel_dix_common_inc.sh # verify()


dix_repair_test()
{
	local fail_device1=2

	echo "Starting DIX repair testing ..."

#	echo "Destroy Meta"
#	$DIXINIT_TOOL_DESTROY >/dev/null
#	echo "Create Meta"
#	$DIXINIT_TOOL_CREATE >/dev/null
	echo "*** m0dixinit is omitted. Mkfs creates meta indices now."

	verify || return $?
	#######################################################################
	#  Now starting DIX repair/rebalance abort/continue testing     #
	#######################################################################

	echo "Set Failure device: $fail_device1"
	cas_disk_state_set "failed" $fail_device1 || return $?

	echo "Device $fail_device1 failed. Do dgmode read"
	verify || return $?

	cas_disk_state_get $fail_device1 || return $?

	echo "Start DIX repair (1)."
	cas_disk_state_set "repair" $fail_device1 || return $?
	dix_repair
	sleep 2

	echo "Abort DIX repair (1)."
	dix_repair_abort

	echo "Wait for DIX repair (1)."
	wait_for_dix_repair_or_rebalance "repair" || return $?
	verify || return $?

	echo "Start DIX repair again (2).."
	dix_repair
	sleep 3

	cas_disk_state_set "repaired" $fail_device1 || return $?
	echo "DIX Repair done."
	verify || return $?

	cas_disk_state_get $fail_device1 || return $?

	cas_disk_state_set "rebalance" $fail_device1 || return $?
	echo "Start DIX Re-balance (1)."
	dix_rebalance
	sleep 2

	echo "Abort DIX Re-balance (1)."
	dix_rebalance_abort

	echo "Wait for DIX Re-balance (1)."
	wait_for_dix_repair_or_rebalance "rebalance" || return $?
	verify || return $?

	echo "Start DIX Re-balance again (2)."
	dix_rebalance
	sleep 2

	echo "Wait for DIX Re-balance (2)."
	wait_for_dix_repair_or_rebalance "rebalance" || return $?
	verify || return $?

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

	NODE_UUID=$(uuidgen)
	local multiple_pools=0
	motr_service start $multiple_pools "$stride" "$N" "$K" "$S" "$P" || {
		echo "Failed to start Motr Service."
		return 1
	}

	for ((i=0; i < ${#IOSEP[*]}; i++)) ; do
		ios_eps="$ios_eps -S ${lnet_nid}:${IOSEP[$i]}"
	done

	dix_repair_test 2>&1 | tee -a "$MOTR_TEST_LOGFILE"
	if [ "${PIPESTATUS[0]}" -ne 0 ]; then
		echo "Failed: DIX repair failed.."
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
report_and_exit dix-repair $?
