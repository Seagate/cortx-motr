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

set +e

## CAUTION: This path will be removed by superuser.
SANDBOX_DIR=${SANDBOX_DIR:-/var/motr/sandbox.restart_motr_service}

. `dirname "$0"`/common.sh
. `dirname "$0"`/m0t1fs_common_inc.sh
. `dirname "$0"`/m0t1fs_client_inc.sh
. `dirname "$0"`/m0t1fs_server_inc.sh
. `dirname "$0"`/m0t1fs_sns_common_inc.sh

rcancel_sandbox="$MOTR_M0T1FS_TEST_DIR/rcancel_sandbox"
source_file="$rcancel_sandbox/rcancel_source"

rcancel_motr_service_start()
{
	local multiple_pools=${1:-0}

	motr_service start "$multiple_pools"
	if [ $? -ne 0 ]
	then
		echo "Failed to start Motr Service"
		return 1
	fi
	echo "motr service started"
}

main()
{
	NODE_UUID=`uuidgen`
	local rc

	sandbox_init

	echo "*********************************************************"
	echo "Start: 1. Motr service start-stop"
	echo "*********************************************************"
	rcancel_motr_service_start || return 1
	motr_service stop
	if [ $? -ne "0" ]
	then
		echo "Failed to stop Motr Service."
		if [ "$rc" -eq "0" ]; then
			return 1
		fi
	fi

	echo "Call unprepare"
	unprepare
	echo "Done with unprepare"

	echo "*********************************************************"
	echo "End: 1. Motr service start-stop"
	echo "*********************************************************"

	echo "pgrep m0"
	pgrep m0
	echo "ps -aef | grep m0"
	ps -aef | grep m0

	echo "lsmod | grep m0"
	lsmod | grep m0
	echo "lsmod | grep lnet"
	lsmod | grep lnet

	echo "*********************************************************"
	echo "Start: 2. Motr service start-stop"
	echo "*********************************************************"

	rcancel_motr_service_start 1 || return 1
	motr_service stop
	if [ $? -ne "0" ]
	then
		echo "Failed to stop Motr Service."
		if [ "$rc" -eq "0" ]; then
			return 1
		fi
	fi
	echo "*********************************************************"
	echo "End: 2. Motr service start-stop"
	echo "*********************************************************"

	echo "Test log available at $MOTR_TEST_LOGFILE."
}

trap unprepare EXIT
main
