#!/usr/bin/env bash
#
# Copyright (c) 2022 Seagate Technology LLC and/or its Affiliates
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
# Authors:
#   hua.huang@seagate.com
#


TOPDIR=$(dirname "$0")/../../

. "${TOPDIR}/m0t1fs/linux_kernel/st/common.sh"
. "${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_common_inc.sh"
. "${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_client_inc.sh"
. "${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_server_inc.sh"
. "${TOPDIR}/m0t1fs/linux_kernel/st/m0t1fs_sns_common_inc.sh"


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


do_some_kv_operations()
{
	local rc=0

	local DIX_FID="12345:12345"
	local MOTR_PARAM="-l ${lnet_nid}:$SNS_MOTR_CLI_EP  \
		    -h ${lnet_nid}:$HA_EP -p $PROF_OPT \
		    -f $M0T1FS_PROC_ID -s "
	local KEY=$(basename $(mktemp -u))
	local VAL=$(basename $(mktemp -u))

	echo "Let's create an index and put {somekey:somevalue}"
	run "$M0_SRC_DIR/utils/m0kv" ${MOTR_PARAM} index create "$DIX_FID"

	echo "Please send SIGUSR1 to the following cmd when it is waiting"
	echo "So, the 'put' will send a single cas put request and then panic."
	run "$M0_SRC_DIR/utils/m0kv" ${MOTR_PARAM} index wait /tmp/please_touch_me put "$DIX_FID" "$KEY" "$VAL"

	echo "Now, send SIGUSR1 to the following cmd when it is waiting"
	echo "So, the 'get' will send requests to all replicas, and only the first one succeeds, and others should fail"
	run "$M0_SRC_DIR/utils/m0kv" ${MOTR_PARAM} index wait /tmp/please_touch_me get "$DIX_FID" "$KEY"

	echo "Now, please trigger DTM0 recovery."
	wait_and_exit
	echo "Now, the DTM0 recovery is done."
	echo "Now, send SIGUSR1 to the following cmd when it is waiting"
	echo "So, the 'get' will send requests to all replicas. All cas requests should succeed"
	run "$M0_SRC_DIR/utils/m0kv" ${MOTR_PARAM} index wait /tmp/please_touch_me get "$DIX_FID" "$KEY"


	wait_and_exit
	return $rc
}

motr_dtm0_client_eviction_test()
{
	local rc=0

	echo "Starting Motr FDMI Plugin testing ..."

	echo
	echo
	echo "MOTR is UP."
	echo "Motr client config:"
	echo
	echo "HA addr        : ${lnet_nid}:$HA_EP           "
	echo "Client addr    : ${lnet_nid}:$SNS_MOTR_CLI_EP "
	echo "Profile fid    : $PROF_OPT                    "
	echo "Process fid    : $M0T1FS_PROC_ID              "
	echo
	echo

	do_some_kv_operations || {
		# Make the rc available for the caller and fail the test
		# if kv operations fail.
		rc=$?
		echo "Test failed with error $rc"
	}

	return $rc
}

main()
{
	local rc=0

	sandbox_init

	local multiple_pools=0
	motr_service start "$multiple_pools" "$stride" "$N" "$K" "$S" "$P" || {
		echo "Failed to start Motr Service."
		return 1
	}


	if [[ $rc -eq 0 ]] && ! motr_dtm0_client_eviction_test ; then
		echo "FDMI plugin test failed."
		rc=1
	fi

	motr_service stop || {
		echo "Failed to stop Motr Service."
		rc=1
	}

	if [ $rc -eq 0 ]; then
		sandbox_fini
	fi
	return $rc
}

trap unprepare EXIT
main
report_and_exit "motr_dtm0_client_eviction_test" $?
