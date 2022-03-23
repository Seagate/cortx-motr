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

CM_OP_REPAIR=1
CM_OP_REBALANCE=2
CM_OP_REPAIR_QUIESCE=3
CM_OP_REBALANCE_QUIESCE=4
CM_OP_REPAIR_RESUME=5
CM_OP_REBALANCE_RESUME=6
CM_OP_REPAIR_STATUS=7
CM_OP_REBALANCE_STATUS=8
CM_OP_REPAIR_ABORT=9
CM_OP_REBALANCE_ABORT=10

dix_repair()
{
	local rc=0

	repair_trigger="$M0_SRC_DIR/sns/cm/st/m0repair -O $CM_OP_REPAIR -t 1 -C ${lnet_nid}:${DIX_CLI_EP} $ios_eps"
	run "$repair_trigger"
	rc=$?
	if [ $rc != 0 ]; then
		echo "DIX Repair failed. rc=$rc"
	fi

	return $rc
}

dix_rebalance()
{
	local rc=0

        rebalance_trigger="$M0_SRC_DIR/sns/cm/st/m0repair -O $CM_OP_REBALANCE -t 1 -C ${lnet_nid}:${DIX_CLI_EP} $ios_eps"
	run "$rebalance_trigger"
	rc=$?
        if [ $rc != 0 ] ; then
                echo "DIX Re-balance failed. rc=$rc"
        fi

	return $rc
}

dix_repair_quiesce()
{
	local rc=0

	repair_quiesce_trigger="$M0_SRC_DIR/sns/cm/st/m0repair -O $CM_OP_REPAIR_QUIESCE -t 1 -C ${lnet_nid}:${DIX_QUIESCE_CLI_EP} $ios_eps"
	run "$repair_quiesce_trigger"
	rc=$?
	if [ $rc != 0 ]; then
		echo "DIX Repair quiesce failed. rc=$rc"
	fi

	return $rc
}

dix_rebalance_quiesce()
{
	local rc=0

	rebalance_quiesce_trigger="$M0_SRC_DIR/sns/cm/st/m0repair -O $CM_OP_REBALANCE_QUIESCE -t 1 -C ${lnet_nid}:${DIX_QUIESCE_CLI_EP} $ios_eps"
	run "$rebalance_quiesce_trigger"
	rc=$?
	if [ $rc != 0 ] ; then
		echo "DIX Re-balance quiesce failed. rc=$rc"
	fi

	return $rc
}

dix_repair_resume()
{
	local rc=0

	repair_resume_trigger="$M0_SRC_DIR/sns/cm/st/m0repair -O $CM_OP_REPAIR_RESUME -t 1 -C ${lnet_nid}:${DIX_CLI_EP} $ios_eps"
	run "$repair_resume_trigger"
	rc=$?
	if [ $rc != 0 ]; then
		echo "DIX Repair resume failed. rc=$rc"
	fi

	return $rc
}

dix_rebalance_resume()
{
	local rc=0

	rebalance_resume_trigger="$M0_SRC_DIR/sns/cm/st/m0repair -O $CM_OP_REBALANCE_RESUME -t 1 -C ${lnet_nid}:${DIX_CLI_EP} $ios_eps"
	run "$rebalance_resume_trigger"
	rc=$?
	if [ $rc != 0 ]; then
		echo "DIX Rebalance resume failed. rc=$rc"
	fi

	return $rc
}

dix_rebalance_abort()
{
	local rc=0

	rebalance_abort_trigger="$M0_SRC_DIR/sns/cm/st/m0repair -O $CM_OP_REBALANCE_ABORT -t 1 -C ${lnet_nid}:${DIX_QUIESCE_CLI_EP} $ios_eps"
	run "$rebalance_abort_trigger"
	rc=$?
	if [ $rc != 0 ] ; then
		echo "DIX Re-balance abort failed. rc=$rc"
	fi

	return $rc
}

dix_repair_abort()
{
	local rc=0

	repair_abort_trigger="$M0_SRC_DIR/sns/cm/st/m0repair -O $CM_OP_REPAIR_ABORT -t 1 -C ${lnet_nid}:${DIX_QUIESCE_CLI_EP} $ios_eps"
	run "$repair_abort_trigger"
	rc=$?
	if [ $rc != 0 ]; then
		echo "DIX Repair abort failed. rc=$rc"
	fi

	return $rc
}

dix_repair_or_rebalance_status_not_4()
{
	local ios_eps_not_4=''
	local rc=0
	local op=32
	[ "$1" == "repair" ] && op=$CM_OP_REPAIR_STATUS
	[ "$1" == "rebalance" ] && op=$CM_OP_REBALANCE_STATUS

	for ((i=0; i < 3; i++)) ; do
		ios_eps_not_4="$ios_eps_not_4 -S ${lnet_nid}:${IOSEP[$i]}"
	done

	repair_status="$M0_SRC_DIR/sns/cm/st/m0repair -O $op -t 1 -C ${lnet_nid}:${DIX_QUIESCE_CLI_EP} $ios_eps_not_4"
	run "$repair_status"
	rc=$?
	if [ $rc != 0 ]; then
		echo "DIX $1 status query failed. rc=$rc"
	fi

	return $rc
}


dix_repair_or_rebalance_status()
{
	local rc=0
	local op=32
	[ "$1" == "repair" ] && op=$CM_OP_REPAIR_STATUS
	[ "$1" == "rebalance" ] && op=$CM_OP_REBALANCE_STATUS

	repair_status="$M0_SRC_DIR/sns/cm/st/m0repair -O $op -t 1 -C ${lnet_nid}:${DIX_QUIESCE_CLI_EP} $ios_eps"
	run "$repair_status"
	rc=$?
	if [ $rc != 0 ]; then
		echo "DIX $1 status query failed. rc=$rc"
	fi

	return $rc
}

wait_for_dix_repair_or_rebalance_not_4()
{
	local ios_eps_not_4=''
	local rc=0
	local op=32
	[ "$1" == "repair" ] && op=$CM_OP_REPAIR_STATUS
	[ "$1" == "rebalance" ] && op=$CM_OP_REBALANCE_STATUS
	for ((i=0; i < 3; i++)) ; do
		ios_eps_not_4="$ios_eps_not_4 -S ${lnet_nid}:${IOSEP[$i]}"
	done
	while true ; do
		sleep 5
		repair_status="$M0_SRC_DIR/sns/cm/st/m0repair -O $op -t 1 -C ${lnet_nid}:${DIX_QUIESCE_CLI_EP} $ios_eps_not_4"

		echo "$repair_status"
		status=$(eval "$repair_status")
		rc=$?
		if [ $rc != 0 ]; then
			echo "DIX $1 status query failed. rc=$rc"
			return $rc
		fi

		echo "$status" | grep status=2 && continue #DIX repair is active, continue waiting
		break;
	done
	return 0
}

wait_for_dix_repair_or_rebalance()
{
	local rc=0
	local op=32
	[ "$1" == "repair" ] && op=$CM_OP_REPAIR_STATUS
	[ "$1" == "rebalance" ] && op=$CM_OP_REBALANCE_STATUS
	while true ; do
		sleep 5
		repair_status="$M0_SRC_DIR/sns/cm/st/m0repair -O $op -t 1 -C ${lnet_nid}:${DIX_QUIESCE_CLI_EP} $ios_eps"
		echo "$repair_status"
		status=$(eval "$repair_status")
		rc=$?
		if [ $rc != 0 ]; then
			echo "DIX $1 status query failed. rc=$rc"
			return $rc
		fi

		echo "$status" | grep status=2 && continue #DIX repair is active, continue waiting
		break;
	done

	op=$(echo "$status" | grep status=3)
	[[ !  -z  "$op"  ]] && return 1

	return 0
}
