#!/bin/bash
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


# Script for starting Motr system tests in "scripts/m0 run-st"

#set -x

motr_st_util_dir=`dirname $0`
m0t1fs_st_dir=$motr_st_util_dir/../../../m0t1fs/linux_kernel/st

# Re-use as many m0t1fs system scripts as possible
. $m0t1fs_st_dir/common.sh
. $m0t1fs_st_dir/m0t1fs_common_inc.sh
. $m0t1fs_st_dir/m0t1fs_client_inc.sh
. $m0t1fs_st_dir/m0t1fs_server_inc.sh
. $m0t1fs_st_dir/m0t1fs_sns_common_inc.sh

# Import wrapper for Motr ST framework which can be run with other
# motr tests or can be used separately.
. $motr_st_util_dir/motr_local_conf.sh
. $motr_st_util_dir/motr_st_inc.sh

# Set the mode of Motr [user|kernel]
umod=1

motr_st_run_tests()
{
	# Start the tests
	if [ $umod -eq 1 ]; then
		motr_st_start_u
		motr_st_stop_u
	else
		motr_st_start_k
		motr_st_stop_k
	fi
}

motr_st_set_failed_dev()
{
	disk_state_set "failed" $1 || {
		echo "Failed: pool_mach_set_failure..."
		return 1
	}
	disk_state_get $1
}

motr_st_dgmode()
{
	NODE_UUID=`uuidgen`
	multiple_pools_flag=0
	motr_service start $multiple_pools_flag
	if [ $? -ne "0" ]
	then
		echo "Failed to start Motr Service."
		return 1
	fi

	#local mountopt="oostore,verify"
	mount_m0t1fs $MOTR_M0T1FS_MOUNT_DIR $mountopt || return 1
	# Inject failure to device 1
	fail_device=1
	motr_st_set_failed_dev $fail_device || {
		return 1
	}

	unmount_and_clean &>> $MOTR_TEST_LOGFILE
	# Run tests
	motr_st_run_tests
	rc=$?

	# motr_service stop --collect-addb
	motr_service stop
	if [ $? -ne "0" ]
	then
		echo "Failed to stop Motr Service."
		return 1
	fi

	if [ $rc -ne "0" ]
	then
		echo "Failed m0t1fs system tests."
		return 1
	fi

	echo "System tests status: SUCCESS."

}

motr_st()
{
	NODE_UUID=`uuidgen`
	multiple_pools_flag=0
	motr_service start $multiple_pools_flag
	if [ $? -ne "0" ]
	then
		echo "Failed to start Motr Service."
		return 1
	fi

	motr_st_run_tests
	rc=$?

	# motr_service stop --collect-addb
	motr_service stop
	if [ $? -ne "0" ]
	then
		echo "Failed to stop Motr Service."
		return 1
	fi

	if [ $rc -ne "0" ]
	then
		echo "Failed Motr system tests."
		return 1
	fi

	echo "System tests status: SUCCESS."
}

main()
{
	local rc

	mkdir -p $MOTR_M0T1FS_TEST_DIR
	echo "Motr system tests start:"
	echo "Test log will be stored in $MOTR_TEST_LOGFILE."

	set -o pipefail

	umod=1
	echo -n "Start Motr Tests [User Mode] ... "
	motr_st $umod -2>&1 | tee -a $MOTR_TEST_LOGFILE
	rc=$?
	echo "Done"

	echo -n "Start Motr Degraded mode Tests [User Mode] ... "
	motr_st_dgmode $umod 2>&1 | tee -a $MOTR_TEST_LOGFILE
	rc=$?
	echo "Done"

	umod=0
	echo -n "Start Motr Tests [Kernel Mode] ... "
	motr_st $umod -2>&1 | tee -a $MOTR_TEST_LOGFILE
	rc=$?
	echo "Done"

	echo -n "Start Motr Degraded mode Tests [Kernel Mode] ... "
	motr_st_dgmode $umod 2>&1 | tee -a $MOTR_TEST_LOGFILE
	rc=$?
	echo "Done"

	echo "Test log available at $MOTR_TEST_LOGFILE."

	return $rc
}

trap unprepare EXIT
main


# Local variables:
# sh-basic-offset: 8
# sh-indentation: 8
# tab-width: 8
# End:
