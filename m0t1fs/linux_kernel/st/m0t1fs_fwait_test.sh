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

#
# Tests the fwait feature on Motr.
#

. `dirname "$0"`/common.sh
. `dirname "$0"`/m0t1fs_common_inc.sh
. `dirname "$0"`/m0t1fs_client_inc.sh
. `dirname "$0"`/m0t1fs_server_inc.sh
. `dirname "$0"`/m0t1fs_sns_common_inc.sh

fwait_test()
{
	`dirname "$0"`/m0t1fs_fwait_test_helper "$MOTR_M0T1FS_MOUNT_DIR"
	return $?
}

main()
{
	sandbox_init

	NODE_UUID=`uuidgen`
	echo "About to start Motr service"
	local multiple_pools=0
	motr_service start $multiple_pools
	if [ $? -ne "0" ]
	then
		echo "Failed to start Motr Service."
		return 1
	fi

	rc=0
	echo "motr service started"

	mkdir -p "$MOTR_M0T1FS_MOUNT_DIR"
	mount_m0t1fs "$MOTR_M0T1FS_MOUNT_DIR" "oostore" || return 1

	fwait_test || {
		echo "Failed: Fwait test failed.."
		rc=1
	}

	unmount_m0t1fs "$MOTR_M0T1FS_MOUNT_DIR" &>> "$MOTR_TEST_LOGFILE"

	motr_service stop
	if [ $? -ne "0" ]
	then
		echo "Failed to stop Motr Service."
		return 1
	fi

	if [ $rc -eq 0 ]; then
		sandbox_fini
	else
		echo "Test log available at $MOTR_TEST_LOGFILE."
	fi
	return $rc
}

trap unprepare EXIT
main
report_and_exit fwait $?
