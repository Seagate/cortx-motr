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


. `dirname "$0"`/common.sh
. `dirname "$0"`/m0t1fs_common_inc.sh
. `dirname "$0"`/m0t1fs_client_inc.sh
. `dirname "$0"`/m0t1fs_server_inc.sh

pool_mach_test()
{
	local ios_eps
	rc=0

	echo "Testing pool machine.."
	for ((i=0; i < ${#IOSEP[*]}; i++)) ; do
		ios_eps="$ios_eps -S ${lnet_nid}:${IOSEP[$i]}"
	done

####### Query
	trigger="$M0_SRC_DIR/pool/m0poolmach -O Query -N 1 -I 'k|1:1'
                         -C ${lnet_nid}:${POOL_MACHINE_CLI_EP} $ios_eps"
	echo "$trigger"
	eval "$trigger"
	rc=$?
	if [ $rc != 0 ] ; then
		echo "m0poolmach failed: $rc"
	else
		echo "m0poolmach done."
	fi

####### Set
	trigger="$M0_SRC_DIR/pool/m0poolmach -O Set -N 1 -I 'k|1:1' -s 2
                         -C ${lnet_nid}:${POOL_MACHINE_CLI_EP} $ios_eps"
	echo "$trigger"
	eval "$trigger"
	rc=$?
	if [ $rc != 0 ] ; then
		echo "m0poolmach failed: $rc"
	else
		echo "m0poolmach done."
	fi

####### Query again
	trigger="$M0_SRC_DIR/pool/m0poolmach -O Query -N 1 -I 'k|1:1'
                         -C ${lnet_nid}:${POOL_MACHINE_CLI_EP} $ios_eps"
	echo "$trigger"
	eval "$trigger"
	rc=$?
	if [ $rc != 0 ] ; then
		echo "m0poolmach failed: $rc"
	else
		echo "m0poolmach done."
	fi

####### Set again. This set request should get error
	trigger="$M0_SRC_DIR/pool/m0poolmach -O Set -N 1 -I 'k|1:1' -s 1
                         -C ${lnet_nid}:${POOL_MACHINE_CLI_EP} $ios_eps"
	echo "$trigger"
	eval "$trigger"
	rc=$?
	if [ $rc != 0 ] ; then
		echo "m0poolmach failed: $rc"
	else
		echo "m0poolmach done."
	fi

####### Set again. This set request should get error
	trigger="$M0_SRC_DIR/pool/m0poolmach -O Set -N 1 -I 'k|1:1' -s 2
                         -C ${lnet_nid}:${POOL_MACHINE_CLI_EP} $ios_eps"
	echo "$trigger"
	eval "$trigger"
	rc=$?
	if [ $rc != 0 ] ; then
		echo "m0poolmach failed: $rc"
	else
		echo "m0poolmach done."
	fi

####### Set again. This set request should get error
	trigger="$M0_SRC_DIR/pool/m0poolmach -O Set -N 1 -I 'k|1:1' -s 3
                         -C ${lnet_nid}:${POOL_MACHINE_CLI_EP} $ios_eps"
	echo "$trigger"
	eval "$trigger"
	rc=$?
	if [ $rc != 0 ] ; then
		echo "m0poolmach failed: $rc"
	else
		echo "m0poolmach done."
	fi

####### Query again
	trigger="$M0_SRC_DIR/pool/m0poolmach -O Query -N 1 -I 'k|1:1'
                         -C ${lnet_nid}:${POOL_MACHINE_CLI_EP} $ios_eps"
	echo "$trigger"
	eval "$trigger"
	rc=$?
	if [ $rc != 0 ] ; then
		echo "m0poolmach failed: $rc"
	else
		echo "m0poolmach done."
	fi

	return $rc
}

main()
{
	sandbox_init

	NODE_UUID=`uuidgen`
	local multiple_pools=0
	motr_service start $multiple_pools
	if [ $? -ne "0" ]
	then
		echo "Failed to start Motr Service."
		return 1
	fi

	rc=0
	pool_mach_test || {
		echo "Failed: pool machine failure."
		rc=1
	}

	motr_service stop
	if [ $? -ne 0 ]; then
		echo "Failed to stop Motr Service."
		return 1
	fi

	sandbox_fini $rc
	return $rc
}

trap unprepare EXIT
main
report_and_exit poolmach $?
