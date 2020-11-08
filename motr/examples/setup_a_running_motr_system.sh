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


TOPDIR="$(dirname "$0")/../.."

. "${TOPDIR}/m0t1fs/linux_kernel/st/common.sh"
. "${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_common_inc.sh"
. "${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_client_inc.sh"
. "${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_server_inc.sh"
. "${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_sns_common_inc.sh"
. "${TOPDIR}/motr/st/utils/sns_repair_common_inc.sh"


export MOTR_CLIENT_ONLY=1

wait_and_exit()
{
	while [ true ] ; do
		echo "Please type EXIT or QUIT to quit"
		read keystroke
		if [ "$keystroke" == "EXIT" -o "$keystroke" == "QUIT" ] ; then
			return 0
		fi
	done
}

show_config_and_wait()
{
	echo
	echo
	echo
	echo
	echo "MOTR is UP."
	echo "Motr client config:"
	echo
	echo "HA_addr    : ${lnet_nid}:$HA_EP           "
	echo "Client_addr: ${lnet_nid}:$SNS_MOTR_CLI_EP "
	echo "Profile_fid: $PROF_OPT                    "
	echo "Process_fid: $M0T1FS_PROC_ID              "
	echo
	echo "Now please use another terminal to run the example"
	echo "with the above command line arguments.            "
	echo "Please add double quote \"\" around the arguments "
	echo
	echo
	echo

	wait_and_exit

	return 0
}

main()
{
	local rc=0

	sandbox_init

	local multiple_pools=0
	motr_service start "$multiple_pools" "$stride" "$N" "$K" "$P" || {
		echo "Failed to start Motr Service."
		return 1
	}

	show_config_and_wait

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
