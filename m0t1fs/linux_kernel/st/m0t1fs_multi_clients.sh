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


. `dirname $0`/common.sh
. `dirname $0`/m0t1fs_common_inc.sh
. `dirname $0`/m0t1fs_client_inc.sh
. `dirname $0`/m0t1fs_server_inc.sh

multi_clients()
{
	NODE_UUID=`uuidgen`
	local multiple_pools=0

	motr_service start $multiple_pools
	if [ $? -ne "0" ]
	then
		echo "Failed to start Motr Service."
		return 1
	fi

	multi_client_test
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
		echo "Failed m0t1fs multi-clients tests: rc=$rc"
		return $rc
	fi
}

main()
{
	sandbox_init

	echo "Starting multi clients testing:"
	echo "Test log will be stored in $MOTR_TEST_LOGFILE."

	multi_clients 2>&1 | tee -a "$MOTR_TEST_LOGFILE"
	rc=${PIPESTATUS[0]}

	if [ $rc -eq 0 ]; then
		sandbox_fini
	else
		echo "Test log available at $MOTR_TEST_LOGFILE."
	fi
	return $rc
}

trap unprepare EXIT
main
report_and_exit multi-client $?
